/*
 *
 * SDPeek - SD Card File Explorer for Teensy
 * 
 * A minimal SD card file browser for Teensy microcontrollers.
 * Provides VT100-esque terminal interface for browsing/viewing SD card contents.
 * 
 * B. Datsko 2024
 * Version: 1.0.0
 * Target: Teensy 4.1
 * 
 */

#include <Arduino.h>
#include <SD.h>
#include <SPI.h>

// Configuration
#define SERIAL_BAUD 2000000
#define MAX_FILE_PREVIEW 1000  // Maximum bytes to preview in cat command
#define SERIAL_TIMEOUT 5000    // Milliseconds to wait for serial connection
#define VERSION "1.0.0"

// Global state
String currentPath = "/";

// Error handling
enum class Error {
    NONE,
    FILE_NOT_FOUND,
    NOT_A_DIRECTORY,
    INVALID_PATH,
    SD_INIT_FAILED,
    REMOVE_FAILED,
    IS_DIRECTORY, 
    NOT_EMPTY   
};

// Forward declarations
void processCommand(const String& cmd);
Error changeDirectory(const String& path);
Error listDirectory(File& dir);
Error printFile(const String& path);
Error removeFile(const String& path);
Error removeDirectory(const String& path);
bool confirmAction(const String& action);

// Utility functions
String formatSize(uint32_t bytes) {
    const double KB = 1024.0;
    const double MB = KB * 1024.0;
    const double GB = MB * 1024.0;
    
    if (bytes < KB) return String(bytes) + " B";
    if (bytes < MB) return String(bytes / KB, 1) + " KB";
    if (bytes < GB) return String(bytes / MB, 1) + " MB";
    return String(bytes / GB, 1) + " GB";
}

void showBanner() {
    SerialUSB.println(F("\n"
        "==========================================\n"
        "              SDPeek v" VERSION "               \n"
        "       SD Card Explorer for Teensy      \n"
        "==========================================\n"
    ));
}

void showHelp() {
    SerialUSB.println(F("\nAvailable commands:"));
    SerialUSB.println(F("  ls              - List files in current directory"));
    SerialUSB.println(F("  pwd             - Print working directory"));
    SerialUSB.println(F("  cd <path>       - Change directory"));
    SerialUSB.println(F("  cat <file>      - Display file contents"));
    SerialUSB.println(F("  free            - Show SD card space"));
    SerialUSB.println(F("  rm <file>       - Remove a file"));
    SerialUSB.println(F("  rmdir <dir>     - Remove an empty directory"));
    SerialUSB.println(F("  help            - Show this help message"));
    SerialUSB.println(F("\nExamples:"));
    SerialUSB.println(F("  cd /            - Go to root directory"));
    SerialUSB.println(F("  cd ..           - Go up one directory"));
    SerialUSB.println(F("  cat data.csv    - Show contents of data.csv"));
}

Error changeDirectory(const String& path) {
    String newPath = path;
    
    if (path == "/") {
        currentPath = "/";
        return Error::NONE;
    }
    
    if (path == "..") {
        if (currentPath == "/") return Error::NONE;
        
        int lastSlash = currentPath.lastIndexOf('/', currentPath.length() - 2);
        if (lastSlash == -1) {
            currentPath = "/";
        } else {
            currentPath = currentPath.substring(0, lastSlash + 1);
        }
        return Error::NONE;
    }

    if (!path.startsWith("/")) {
        newPath = currentPath + path;
    }
    
    if (!newPath.endsWith("/")) {
        newPath += "/";
    }

    File dir = SD.open(newPath.c_str());
    if (!dir) {
        return Error::FILE_NOT_FOUND;
    }
    if (!dir.isDirectory()) {
        dir.close();
        return Error::NOT_A_DIRECTORY;
    }
    dir.close();

    currentPath = newPath;
    return Error::NONE;
}

bool confirmAction(const String& action) {
    SerialUSB.print("Are you sure you want to " + action + "? (y/N): ");
    while (!SerialUSB.available()) {}
    
    String response = SerialUSB.readStringUntil('\n');
    response.trim();
    response.toLowerCase();
    
    return response == "y" || response == "yes";
}

Error removeFile(const String& path) {
    File file = SD.open(path.c_str());
    if (!file) return Error::FILE_NOT_FOUND;
    
    if (file.isDirectory()) {
        file.close();
        return Error::IS_DIRECTORY;
    }
    file.close();
    
    if (!confirmAction("delete " + String(path))) {
        return Error::NONE;
    }
    
    if (!SD.remove(path.c_str())) {
        return Error::REMOVE_FAILED;
    }
    
    return Error::NONE;
}

Error removeDirectory(const String& path) {
    File dir = SD.open(path.c_str());
    if (!dir) return Error::FILE_NOT_FOUND;
    
    if (!dir.isDirectory()) {
        dir.close();
        return Error::NOT_A_DIRECTORY;
    }
    
    // Check if directory is empty
    File entry = dir.openNextFile();
    if (entry) {
        entry.close();
        dir.close();
        return Error::NOT_EMPTY;
    }
    dir.close();
    
    if (!confirmAction("remove directory " + String(path))) {
        return Error::NONE;
    }
    
    if (!SD.rmdir(path.c_str())) {
        return Error::REMOVE_FAILED;
    }
    
    return Error::NONE;
}

Error listDirectory(File& dir) {
    if (!dir) return Error::FILE_NOT_FOUND;
    
    while (true) {
        File entry = dir.openNextFile();
        if (!entry) break;
        
        if (entry.isDirectory()) {
            SerialUSB.print("+ ");
            SerialUSB.print(entry.name());
            SerialUSB.println("/");
        } else {
            SerialUSB.print("  ");
            SerialUSB.print(entry.name());
            SerialUSB.print("  ");
            SerialUSB.println(formatSize(entry.size()));
        }
        
        entry.close();
    }
    return Error::NONE;
}

Error printFile(const String& path) {
    File file = SD.open(path.c_str());
    if (!file) return Error::FILE_NOT_FOUND;
    
    if (file.isDirectory()) {
        file.close();
        return Error::NOT_A_DIRECTORY;
    }
    
    SerialUSB.println("\n=== File: " + path + " ===");
    
    uint32_t bytesRead = 0;
    while (file.available() && bytesRead < MAX_FILE_PREVIEW) {
        SerialUSB.write(file.read());
        bytesRead++;
    }
    
    if (file.available()) {
        SerialUSB.println("\n\n[Output truncated... File size: " + 
                         formatSize(file.size()) + "]");
    }
    
    file.close();
    return Error::NONE;
}

void showFreeSpace() {
    File root = SD.open("/");
    if (root) {
        uint64_t totalSpace = root.size();
        SerialUSB.println("\nSD Card Information:");
        SerialUSB.println("-------------------");
        SerialUSB.print("Total Size: ");
        SerialUSB.println(formatSize(totalSpace));
        root.close();
    }
}

void processCommand(const String& cmd) {
    if (cmd == "ls") {
        SerialUSB.println("\nDirectory listing of " + currentPath + ":");
        SerialUSB.println("------------------");
        File dir = SD.open(currentPath.c_str());
        Error err = listDirectory(dir);
        dir.close();
        
        if (err != Error::NONE) {
            SerialUSB.println("Error: Failed to list directory");
        }
    }
    else if (cmd == "pwd") {
        SerialUSB.println(currentPath);
    }
    else if (cmd.startsWith("rm ")) {
        String path = cmd.substring(3);
        path.trim();
        
        if (!path.startsWith("/")) {
            path = currentPath + path;
        }
        
        Error err = removeFile(path);
        if (err == Error::FILE_NOT_FOUND) {
            SerialUSB.println("Error: File not found");
        } else if (err == Error::IS_DIRECTORY) {
            SerialUSB.println("Error: Is a directory, use rmdir instead");
        } else if (err == Error::REMOVE_FAILED) {
            SerialUSB.println("Error: Failed to remove file");
        } else if (err == Error::NONE) {
            SerialUSB.println("File removed successfully");
        }
    }
    else if (cmd.startsWith("rmdir ")) {
        String path = cmd.substring(6);
        path.trim();
        
        if (!path.startsWith("/")) {
            path = currentPath + path;
        }
        
        Error err = removeDirectory(path);
        if (err == Error::FILE_NOT_FOUND) {
            SerialUSB.println("Error: Directory not found");
        } else if (err == Error::NOT_A_DIRECTORY) {
            SerialUSB.println("Error: Not a directory");
        } else if (err == Error::NOT_EMPTY) {
            SerialUSB.println("Error: Directory not empty");
        } else if (err == Error::REMOVE_FAILED) {
            SerialUSB.println("Error: Failed to remove directory");
        } else if (err == Error::NONE) {
            SerialUSB.println("Directory removed successfully");
        }
    }
    else if (cmd.startsWith("cd ")) {
        String path = cmd.substring(3);
        path.trim();
        Error err = changeDirectory(path);
        
        if (err == Error::FILE_NOT_FOUND) {
            SerialUSB.println("Error: Directory not found");
        } else if (err == Error::NOT_A_DIRECTORY) {
            SerialUSB.println("Error: Not a directory");
        }
    }
    else if (cmd.startsWith("cat ")) {
        String path = cmd.substring(4);
        path.trim();
        
        if (!path.startsWith("/")) {
            path = currentPath + path;
        }
        
        Error err = printFile(path);
        if (err == Error::FILE_NOT_FOUND) {
            SerialUSB.println("Error: File not found");
        }
    }
    else if (cmd == "free") {
        showFreeSpace();
    }
    else if (cmd == "help") {
        showHelp();
    }
    else if (cmd.length() > 0) {
        SerialUSB.println("Unknown command. Type 'help' for available commands.");
    }
    
    SerialUSB.print("\n> ");
}

void setup() {
    SerialUSB.begin(SERIAL_BAUD);
    
    // Wait for serial connection
    unsigned long startTime = millis();
    while (!SerialUSB && (millis() - startTime) < SERIAL_TIMEOUT) {
        delay(100);
    }

    showBanner();
    
    if (!SD.begin(BUILTIN_SDCARD)) {
        SerialUSB.println("Error: SD card initialization failed!");
        return;
    }
    
    SerialUSB.println("SD card initialized successfully.");
    SerialUSB.println("Type 'help' for available commands.");
    SerialUSB.print("\n> ");
}

void loop() {
    if (SerialUSB.available()) {
        String cmd = SerialUSB.readStringUntil('\n');
        cmd.trim();
        processCommand(cmd);
    }
}
