#include <Arduino.h>
#include <SD.h>
#include <SPI.h>
#include <map>

#define SERIAL_BAUD 2000000
#define MAX_FILE_PREVIEW 1000
#define SERIAL_TIMEOUT 5000
#define VERSION "1.2.1"
#define DEFAULT_SYNC_DIR "/SYNC"

String currentPath = "/";

enum class Error { NONE, FILE_NOT_FOUND, NOT_A_DIRECTORY, INVALID_PATH, SD_INIT_FAILED, REMOVE_FAILED, IS_DIRECTORY, NOT_EMPTY };
Error findFiles(const String& pattern, const String& currentDir = "");
Error countItems(const String& path, unsigned long& fileCount, unsigned long& dirCount);

String formatSize(uint64_t bytes) {
    const char* units[] = {" B", " KB", " MB", " GB"};
    int unitIndex = 0;
    double size = bytes;
    while (size >= 1024 && unitIndex < 3) {
        size /= 1024;
        unitIndex++;
    }
    return String(size, (unitIndex == 0) ? 0 : 1) + units[unitIndex];
}

void showBanner() {
    SerialUSB.println(F("\n=========================================="));
    SerialUSB.println(F("              SDPeek v" VERSION "               "));
    SerialUSB.println(F("       SD Card Explorer for Teensy      "));
    SerialUSB.println(F("==========================================\n"));
}

void showHelp() {
    SerialUSB.println(F("Available commands:"));
    SerialUSB.println(F("  ls              - List files in current directory"));
    SerialUSB.println(F("  pwd             - Print working directory"));
    SerialUSB.println(F("  cd <path>       - Change directory"));
    SerialUSB.println(F("  cat <file>      - Display file contents"));
    SerialUSB.println(F("  free            - Show SD card space"));
    SerialUSB.println(F("  rm <file>       - Remove a file"));
    SerialUSB.println(F("  rmdir <dir>     - Remove an empty directory"));
    SerialUSB.println(F("  syncdir [path]  - Sync files from host (optional custom path)"));
    SerialUSB.println(F("  resync          - Resync files from host to /SYNC directory"));
    SerialUSB.println(F("  foldersummary <path> - Show summary of folder contents"));
    SerialUSB.println(F("  help            - Show this help message"));
    SerialUSB.println(F("  find <pattern>   - Find files matching pattern (case-insensitive)"));
    SerialUSB.println(F("  count            - Count files and directories in current path"));
 
}
Error receiveFile(const String& path);
void printProgress(unsigned long current, unsigned long total);


Error changeDirectory(const String& path) {
    String newPath = path.startsWith("/") ? path : currentPath + path;
    if (newPath == "/") {
        currentPath = "/";
        return Error::NONE;
    }
    if (newPath == "..") {
        int lastSlash = currentPath.lastIndexOf('/', currentPath.length() - 2);
        currentPath = (lastSlash == -1) ? "/" : currentPath.substring(0, lastSlash + 1);
        return Error::NONE;
    }
    if (!newPath.endsWith("/")) newPath += "/";
    File dir = SD.open(newPath.c_str());
    if (!dir) return Error::FILE_NOT_FOUND;
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
    return response.toLowerCase().startsWith("y");
}

Error removeFile(const String& path) {
    File file = SD.open(path.c_str());
    if (!file) return Error::FILE_NOT_FOUND;
    if (file.isDirectory()) {
        file.close();
        return Error::IS_DIRECTORY;
    }
    file.close();
    if (!confirmAction("delete " + path)) return Error::NONE;
    return SD.remove(path.c_str()) ? Error::NONE : Error::REMOVE_FAILED;
}



unsigned long countFilesRecursive(File dir) {
    unsigned long count = 0;
    File entry = dir.openNextFile();
    
    while (entry) {
        if (entry.isDirectory()) {
            count += countFilesRecursive(entry);
        } else {
            count++;
        }
        entry.close();
        entry = dir.openNextFile();
    }
    
    return count;
}

// Add this helper function to get relative path
String getRelativePath(const String& basePath, const String& fullPath) {
    if (fullPath.startsWith(basePath)) {
        return fullPath.substring(basePath.length());
    }
    return fullPath;
}

Error removeDirectory(const String& path) {
    File dir = SD.open(path.c_str());
    if (!dir) return Error::FILE_NOT_FOUND;
    if (!dir.isDirectory()) {
        dir.close();
        return Error::NOT_A_DIRECTORY;
    }
    if (dir.openNextFile()) {
        dir.close();
        return Error::NOT_EMPTY;
    }
    dir.close();
    if (!confirmAction("remove directory " + path)) return Error::NONE;
    return SD.rmdir(path.c_str()) ? Error::NONE : Error::REMOVE_FAILED;
}

Error listDirectory(File& dir) {
    if (!dir) return Error::FILE_NOT_FOUND;
    File entry;
    while (entry = dir.openNextFile()) {
        SerialUSB.print(entry.isDirectory() ? "+ " : "  ");
        SerialUSB.print(entry.name());
        if (entry.isDirectory()) SerialUSB.println("/");
        else SerialUSB.println("  " + formatSize(entry.size()));
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
        SerialUSB.println("\n\n[Output truncated... File size: " + formatSize(file.size()) + "]");
    }
    file.close();
    return Error::NONE;
}
Error sendDirectory(const String& path) {
    File dir = SD.open(path.c_str());
    if (!dir || !dir.isDirectory()) return Error::NOT_A_DIRECTORY;

    // First, count all files recursively
    unsigned long fileCount = countFilesRecursive(dir);
    dir.close();

    // Send total file count
    SerialUSB.println(String("DIR_COUNT:") + fileCount);
    
    // If no files found, end early
    if (fileCount == 0) {
        SerialUSB.println("DIR_DONE");
        return Error::NONE;
    }

    // Reopen directory for sending files
    dir = SD.open(path.c_str());
    
    // Helper function to process directory recursively
    std::function<void(File&, const String&)> processDirectory = [&](File& dir, const String& currentPath) {
        File entry = dir.openNextFile();
        while (entry) {
            String entryPath = currentPath + "/" + entry.name();
            
            if (entry.isDirectory()) {
                processDirectory(entry, entryPath);
            } else {
                // Send file info with full relative path
                SerialUSB.println(String("FILE:") + entryPath);
                SerialUSB.println(entry.size());
                
                // Send file contents
                uint32_t remaining = entry.size();
                while (remaining > 0) {
                    uint32_t chunk = min(512UL, remaining);
                    uint8_t buffer[512];
                    entry.read(buffer, chunk);
                    SerialUSB.write(buffer, chunk);
                    remaining -= chunk;
                }
                SerialUSB.println("FILE_DONE");
            }
            entry.close();
            entry = dir.openNextFile();
        }
    };

    // Start recursive processing from root
    processDirectory(dir, "");
    
    dir.close();
    SerialUSB.println("DIR_DONE");
    return Error::NONE;
}



void showFreeSpace() {
    File root = SD.open("/");
    if (root) {
        uint64_t totalSpace = root.size();
        uint64_t usedSpace = 0;
        File file = root.openNextFile();
        while (file) {
            if (!file.isDirectory()) usedSpace += file.size();
            file = root.openNextFile();
        }
        SerialUSB.println("\nSD Card Information:");
        SerialUSB.println("-------------------");
        SerialUSB.println("Total Size: " + formatSize(totalSpace));
        SerialUSB.println("Used Space: " + formatSize(usedSpace));
        SerialUSB.println("Free Space: " + formatSize(totalSpace - usedSpace));
        root.close();
    }
}

Error syncDirectory(const String& localPath, const String& remotePath = DEFAULT_SYNC_DIR) {
    String actualRemotePath = remotePath.length() == 0 ? DEFAULT_SYNC_DIR : remotePath;
    if (!SD.exists(actualRemotePath.c_str()) && !SD.mkdir(actualRemotePath.c_str())) {
        SerialUSB.println("Error: Failed to create sync directory");
        return Error::INVALID_PATH;
    }
    SerialUSB.println("Ready to receive files. Start transfer from host.");
    unsigned long fileCount = 0, processedFiles = 0;
    while (true) {
        String command = SerialUSB.readStringUntil('\n');
        command.trim();
        if (command == "SYNC_COMPLETE") break;
        if (command.startsWith("FILE_COUNT:")) {
            fileCount = command.substring(11).toInt();
            continue;
        }
        if (command.startsWith("FILE:")) {
            String filePath = actualRemotePath + "/" + command.substring(5);
            Error err = receiveFile(filePath);
            if (err != Error::NONE) return err;
            processedFiles++;
            printProgress(processedFiles, fileCount);
        } else {
            SerialUSB.println("Error: Invalid sync command");
            return Error::INVALID_PATH;
        }
    }
    SerialUSB.println("\nSync completed");
    return Error::NONE;
}

Error receiveFile(const String& path) {
    File file = SD.open(path.c_str(), FILE_WRITE);
    if (!file) {
        SerialUSB.println("Error: Unable to create file");
        return Error::FILE_NOT_FOUND;
    }
    uint32_t fileSize = SerialUSB.parseInt();
    SerialUSB.println("Receiving file: " + String(path));
    SerialUSB.read(); // Consume newline
    uint32_t bytesReceived = 0;
    while (bytesReceived < fileSize) {
        if (SerialUSB.available()) {
            file.write(SerialUSB.read());
            bytesReceived++;
        }
    }
    SerialUSB.println("Received " + String(bytesReceived) + " bytes");


    file.close();
    SerialUSB.println("FILE_RECEIVED");
    return Error::NONE;
}

void printProgress(unsigned long current, unsigned long total) {
    int percent = (current * 100) / total;
    SerialUSB.print(F("\rProgress: "));
    SerialUSB.print(percent);
    SerialUSB.print(F("%"));
    SerialUSB.flush();
}

Error folderSummary(const String& path) {
    File dir = SD.open(path.c_str());
    if (!dir || !dir.isDirectory()) return Error::NOT_A_DIRECTORY;

    unsigned long fileCount = 0;
    uint64_t totalSize = 0;
    std::map<String, unsigned long> fileNames;

    File entry;
    while (entry = dir.openNextFile()) {
        if (!entry.isDirectory()) {
            String fileName = entry.name();
            fileCount++;
            totalSize += entry.size();
            fileNames[fileName]++;
        }
        entry.close();
    }

    SerialUSB.println(F("\nFolder Summary:"));
    SerialUSB.println(F("---------------"));
    SerialUSB.println("Total files: " + String(fileCount));
    SerialUSB.println("Total size: " + formatSize(totalSize));

    unsigned long duplicates = 0;
    for (const auto& pair : fileNames) {
        if (pair.second > 1) duplicates++;
    }
    SerialUSB.println("Duplicate files: " + String(duplicates));

    dir.close();
    return Error::NONE;
}

Error findFiles(const String& pattern, const String& currentDir) {
    File dir = SD.open(currentDir.length() > 0 ? currentDir.c_str() : currentPath.c_str());
    if (!dir || !dir.isDirectory()) return Error::NOT_A_DIRECTORY;

    File entry;
    bool foundAny = false;
    String basePath = currentDir.length() > 0 ? currentDir : currentPath;
    
    while (entry = dir.openNextFile()) {
        String entryName = entry.name();
        String fullPath = basePath + entryName;
        
        // Convert both strings to lowercase for case-insensitive comparison
        String lowerPattern = pattern;
        String lowerName = entryName;
        lowerPattern.toLowerCase();
        lowerName.toLowerCase();
        
        // Check if the pattern matches
        if (lowerName.indexOf(lowerPattern) != -1) {
            foundAny = true;
            SerialUSB.print(fullPath);
            if (entry.isDirectory()) {
                SerialUSB.println("/");
            } else {
                SerialUSB.println("  (" + formatSize(entry.size()) + ")");
            }
        }
        
        // Recursively search in subdirectories
        if (entry.isDirectory()) {
            String newPath = fullPath + "/";
            findFiles(pattern, newPath);
        }
        
        entry.close();
    }
    
    dir.close();
    if (!foundAny && currentDir.length() == 0) {
        SerialUSB.println("No matches found for '" + pattern + "'");
    }
    return Error::NONE;
}

Error countItems(const String& path, unsigned long& fileCount, unsigned long& dirCount) {
    File dir = SD.open(path.c_str());
    if (!dir || !dir.isDirectory()) return Error::NOT_A_DIRECTORY;

    fileCount = 0;
    dirCount = 0;
    uint64_t totalSize = 0;

    File entry;
    while (entry = dir.openNextFile()) {
        if (entry.isDirectory()) {
            dirCount++;
        } else {
            fileCount++;
            totalSize += entry.size();
        }
        entry.close();
    }
    
    dir.close();
    
    SerialUSB.println("\nDirectory Count Summary:");
    SerialUSB.println("----------------------");
    SerialUSB.println("Files: " + String(fileCount));
    SerialUSB.println("Directories: " + String(dirCount));
    SerialUSB.println("Total Size: " + formatSize(totalSize));
    
    return Error::NONE;
}

Error clearFolder(const String& path) {
    // Attempt to open the specified path
    File dir = SD.open(path.c_str());
    if (!dir || !dir.isDirectory()) {
        if (dir) dir.close();
        return Error::NOT_A_DIRECTORY;
    }

    // Ask for user confirmation before proceeding
    if (!confirmAction("clear (delete everything in) the directory " + path)) {
        dir.close();
        return Error::NONE;  // User declined, so do nothing
    }

    // Clear out the contents of the directory
    File entry;
    while ((entry = dir.openNextFile())) {
        String entryName = entry.name();
        String fullPath = path;
        if (!fullPath.endsWith("/")) fullPath += "/";
        fullPath += entryName;

        if (entry.isDirectory()) {
            // Close the File object before recursing
            entry.close();
            // Recursively clear this subfolder
            Error err = clearFolder(fullPath);
            if (err != Error::NONE) {
                dir.close();
                return err;
            }
            // Now remove the (empty) directory itself
            if (!SD.rmdir(fullPath.c_str())) {
                dir.close();
                return Error::REMOVE_FAILED;
            }
        } else {
            // Close the File object before removing
            entry.close();
            // Remove file
            if (!SD.remove(fullPath.c_str())) {
                dir.close();
                return Error::REMOVE_FAILED;
            }
        }
    }
    dir.close();

    return Error::NONE;
}



void processCommand(const String& cmd) {
    if (cmd == "banner") { showBanner(); showHelp(); }
    else if (cmd == "ls") {
        SerialUSB.println("\nDirectory listing of " + currentPath + ":");
        SerialUSB.println("------------------");
        File dir = SD.open(currentPath.c_str());
        Error err = listDirectory(dir);
        dir.close();
        if (err != Error::NONE) SerialUSB.println("Error: Failed to list directory");
    }
    else if (cmd == "pwd") SerialUSB.println(currentPath);
    else if (cmd.startsWith("rm ")) {
        String path = cmd.substring(3);
        path.trim();
        if (!path.startsWith("/")) path = currentPath + path;
        Error err = removeFile(path);
        if (err == Error::FILE_NOT_FOUND) SerialUSB.println("Error: File not found");
        else if (err == Error::IS_DIRECTORY) SerialUSB.println("Error: Is a directory, use rmdir instead");
        else if (err == Error::REMOVE_FAILED) SerialUSB.println("Error: Failed to remove file");
        else if (err == Error::NONE) SerialUSB.println("File removed successfully");
    }
       else if (cmd.startsWith("downloaddir ")) {
        String path = cmd.substring(11);
        path.trim();
        if (!path.startsWith("/")) path = currentPath + path;
        Error err = sendDirectory(path);
        if (err != Error::NONE) {
            SerialUSB.println("Error: Failed to send directory");
        }
    }
    else if (cmd.startsWith("rmdir ")) {
        String path = cmd.substring(6);
        path.trim();
        if (!path.startsWith("/")) path = currentPath + path;
        Error err = removeDirectory(path);
        if (err == Error::FILE_NOT_FOUND) SerialUSB.println("Error: Directory not found");
        else if (err == Error::NOT_A_DIRECTORY) SerialUSB.println("Error: Not a directory");
        else if (err == Error::NOT_EMPTY) SerialUSB.println("Error: Directory not empty");
        else if (err == Error::REMOVE_FAILED) SerialUSB.println("Error: Failed to remove directory");
        else if (err == Error::NONE) SerialUSB.println("Directory removed successfully");
    }
      else if (cmd.startsWith("clearfolder ")) {
        String path = cmd.substring(12);
        path.trim();
        if (!path.startsWith("/")) path = currentPath + path;
        Error err = clearFolder(path);
        if (err == Error::NOT_A_DIRECTORY) {
            SerialUSB.println("Error: Not a directory");
        } else if (err == Error::REMOVE_FAILED) {
            SerialUSB.println("Error: Failed to remove files or subdirectories");
        } else if (err == Error::NONE) {
            SerialUSB.println("Directory cleared successfully");
        }
    }
    else if (cmd.startsWith("cd ")) {
        String path = cmd.substring(3);
        path.trim();
        Error err = changeDirectory(path);
        if (err == Error::FILE_NOT_FOUND) SerialUSB.println("Error: Directory not found");
        else if (err == Error::NOT_A_DIRECTORY) SerialUSB.println("Error: Not a directory");
    }
    else if (cmd.startsWith("cat ")) {
        String path = cmd.substring(4);
        path.trim();
        if (!path.startsWith("/")) path = currentPath + path;
        Error err = printFile(path);
        if (err == Error::FILE_NOT_FOUND) SerialUSB.println("Error: File not found");
    }
    else if (cmd == "free") showFreeSpace();
    else if (cmd == "resync") {
        Error err = syncDirectory("", "");
        if (err != Error::NONE) SerialUSB.println("Error: Resync failed");
        else SerialUSB.println("Resync completed successfully");
    }
    else if (cmd.startsWith("syncdir")) {
        String args = cmd.substring(7).trim();
        Error err = syncDirectory("", args);
        if (err != Error::NONE) SerialUSB.println("Error: Sync failed");
        else SerialUSB.println("Sync completed successfully");
    }
    else if (cmd.startsWith("foldersummary ")) {
        String path = cmd.substring(14);
        path.trim();
        if (!path.startsWith("/")) path = currentPath + path;
        Error err = folderSummary(path);
        if (err != Error::NONE) SerialUSB.println("Error: Invalid directory");
    }
     else if (cmd.startsWith("find ")) {
        String pattern = cmd.substring(5);
        pattern.trim();
        if (pattern.length() == 0) {
            SerialUSB.println("Error: Search pattern required");
            return;
        }
        Error err = findFiles(pattern);
        if (err != Error::NONE) {
            SerialUSB.println("Error: Failed to search directory");
        }
    }
    else if (cmd == "count") {
        unsigned long fileCount = 0, dirCount = 0;
        Error err = countItems(currentPath, fileCount, dirCount);
        if (err != Error::NONE) {
            SerialUSB.println("Error: Failed to count items");
        }
    }
    else if (cmd == "help") showHelp();
    else if (cmd.length() > 0) SerialUSB.println("Unknown command. Type 'help' for available commands.");
    SerialUSB.print("\n> ");
}

void setup() {
    SerialUSB.begin(SERIAL_BAUD);
    unsigned long startTime = millis();
    while (!SerialUSB && (millis() - startTime) < SERIAL_TIMEOUT) delay(100);
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

