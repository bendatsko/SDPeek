import os


def main():
    Switch_20_or_50 = 20
    for problems in range(1, 1001):
        if Switch_20_or_50 == 20:
            # Directory
            # directory = "uf20-0" + str(problems)
            directory = "3-SAT_50Var_218Cls_Seed" + str(problems)

            # Parent Directory path
            # parent_dir = os.path.join(os.getcwd(), "uf20-91")
            parent_dir = os.path.join(os.getcwd(), "encoded", "50var_encoded")

            # Path
            path = os.path.join(parent_dir, directory)

            # Create the directory
            # 'GeeksForGeeks' in
            # '/home / User / Documents' #if the directory exists, then it will ignore the error.
            os.makedirs(path, exist_ok=True)

            inputfile_cnf = open(
                os.path.join(
                    os.getcwd(),
                    "50var",
                    "3-SAT_50Var_218Cls_Seed" + str(problems) + ".cnf",
                )
            )
            # outputfile_MEMORY_data0=open('Scan_chain_memory_bank_data0.csv','w')
            # outputfile_MEMORY_data1=open('Scan_chain_memory_bank_data1.csv','w')
            # outputfile_MEMORY_data2=open('Scan_chain_memory_bank_data2.csv','w')
            # outputfile_MEMORY_data0_2=open('Scan_chain_memory_bank_data0_2.csv','w')
            # outputfile_MEMORY_data1_2=open('Scan_chain_memory_bank_data1_2.csv','w')
            # outputfile_MEMORY_data2_2=open('Scan_chain_memory_bank_data2_2.csv','w')
            outputfile_MEMORY_data0 = open(os.path.join(path, "data_info_01.csv"), "w")
            outputfile_MEMORY_data1 = open(os.path.join(path, "data_info_11.csv"), "w")
            outputfile_MEMORY_data2 = open(os.path.join(path, "data_info_21.csv"), "w")
            outputfile_MEMORY_data0_2 = open(
                os.path.join(path, "data_info_02.csv"), "w"
            )
            outputfile_MEMORY_data1_2 = open(
                os.path.join(path, "data_info_12.csv"), "w"
            )
            outputfile_MEMORY_data2_2 = open(
                os.path.join(path, "data_info_22.csv"), "w"
            )
        if Switch_20_or_50 == 50:
            # Directory
            directory = "uf50-0" + str(problems)
            # Parent Directory path
            parent_dir = os.path.join(os.getcwd(), "uf50-218")

            # Path
            path = os.path.join(parent_dir, directory)

            # Create the directory
            # 'GeeksForGeeks' in
            # '/home / User / Documents' #if the directory exists, then it will ignore the error.
            os.makedirs(path, exist_ok=True)

            inputfile_cnf = open(
                os.path.join(os.getcwd(), "uf50-218", "uf50-0" + str(problems) + ".cnf")
            )
            # outputfile_MEMORY_data0=open('Scan_chain_memory_bank_data0.csv','w')
            # outputfile_MEMORY_data1=open('Scan_chain_memory_bank_data1.csv','w')
            # outputfile_MEMORY_data2=open('Scan_chain_memory_bank_data2.csv','w')
            # outputfile_MEMORY_data0_2=open('Scan_chain_memory_bank_data0_2.csv','w')
            # outputfile_MEMORY_data1_2=open('Scan_chain_memory_bank_data1_2.csv','w')
            # outputfile_MEMORY_data2_2=open('Scan_chain_memory_bank_data2_2.csv','w')
            outputfile_MEMORY_data0 = open(os.path.join(path, "data_info_01.csv"), "w")
            outputfile_MEMORY_data1 = open(os.path.join(path, "data_info_11.csv"), "w")
            outputfile_MEMORY_data2 = open(os.path.join(path, "data_info_21.csv"), "w")
            outputfile_MEMORY_data0_2 = open(
                os.path.join(path, "data_info_02.csv"), "w"
            )
            outputfile_MEMORY_data1_2 = open(
                os.path.join(path, "data_info_12.csv"), "w"
            )
            outputfile_MEMORY_data2_2 = open(
                os.path.join(path, "data_info_22.csv"), "w"
            )
        counter = 0
        number_variables = 0
        number_clauses = 0
        print(problems)
        for line in inputfile_cnf:
            line_data = line.split()

            # print(counter)
            # Get the informatino for the cnf file==> # of variables, # of clauses
            if line_data[0] == "c":
                continue
            if line_data[0] == "%":
                break
            if counter == 0:
                number_variables = int(line_data[2])
                number_clauses = int(line_data[3])
                counter += 1
                # Especailly sets this to 50
                number_variables = 50
            # Start to generate the EN and INV array
            else:
                line_data = line_data[0 : len(line_data) - 1]
                abs_line_data = []
                # Make the line_data to become integer and absolute value
                for sort_iter in range(0, len(line_data)):
                    line_data[sort_iter] = int(line_data[sort_iter])
                    abs_line_data.append(abs(int(line_data[sort_iter])))
                # Write Data0
                if line_data[0] < 0:
                    value0 = 2 ** (49 - (abs_line_data[0] - 1)) + 2**51
                    outputfile_MEMORY_data0.write(str(value0 & 0xFFFFFFFF) + ",")
                    outputfile_MEMORY_data0_2.write(
                        str((value0 >> 32) & 0xFFFFFFFF) + ","
                    )
                    # if counter!=228:
                    # outputfile_MEMORY_data0.write("\n")
                    # outputfile_MEMORY_data0_2.write("\n")
                else:
                    value0 = 2 ** (49 - (abs_line_data[0] - 1)) + 2**50 + 2**51
                    outputfile_MEMORY_data0.write(str(value0 & 0xFFFFFFFF) + ",")
                    outputfile_MEMORY_data0_2.write(
                        str((value0 >> 32) & 0xFFFFFFFF) + ","
                    )
                    # if counter!=228:
                    # outputfile_MEMORY_data0.write("\n")
                    # outputfile_MEMORY_data0_2.write("\n")
                # Write Data1
                if line_data[1] < 0:
                    value1 = 2 ** (49 - (abs_line_data[1] - 1)) + 2**51
                    outputfile_MEMORY_data1.write(str(value1 & 0xFFFFFFFF) + ",")
                    outputfile_MEMORY_data1_2.write(
                        str((value1 >> 32) & 0xFFFFFFFF) + ","
                    )
                    # if counter!=228:
                    # outputfile_MEMORY_data1.write("\n")
                    # outputfile_MEMORY_data1_2.write("\n")
                else:
                    value1 = 2 ** (49 - (abs_line_data[1] - 1)) + 2**50 + 2**51
                    outputfile_MEMORY_data1.write(str(value1 & 0xFFFFFFFF) + ",")
                    outputfile_MEMORY_data1_2.write(
                        str((value1 >> 32) & 0xFFFFFFFF) + ","
                    )
                    # if counter!=228:
                    # outputfile_MEMORY_data1.write("\n")
                    # outputfile_MEMORY_data1_2.write("\n")
                # Write Data2
                if line_data[2] < 0:
                    value2 = 2 ** (49 - (abs_line_data[2] - 1)) + 2**51
                    outputfile_MEMORY_data2.write(str(value2 & 0xFFFFFFFF) + ",")
                    outputfile_MEMORY_data2_2.write(
                        str((value2 >> 32) & 0xFFFFFFFF) + ","
                    )
                    # if counter!=228:
                    # outputfile_MEMORY_data2.write("\n")
                    # outputfile_MEMORY_data2_2.write("\n")
                else:
                    value2 = 2 ** (49 - (abs_line_data[2] - 1)) + 2**50 + 2**51
                    outputfile_MEMORY_data2.write(str(value2 & 0xFFFFFFFF) + ",")
                    outputfile_MEMORY_data2_2.write(
                        str((value2 >> 32) & 0xFFFFFFFF) + ","
                    )
                    # if counter!=228:
                    # outputfile_MEMORY_data2.write("\n")
                    # outputfile_MEMORY_data2_2.write("\n")
                # for i in range(0,number_variables):
                # 	#sorting the elements in one clause, also get the abs|elements|
                # 	if i in abs_line_data:
                # 		index = abs_line_data.index(i)
                # 		outputfile_EN.write("1 ")
                # 		if line_data[index] <0:
                # 			outputfile_INV.write("1 ")
                # 		else:
                # 			outputfile_INV.write("0 ")
                # 	else:
                # 		outputfile_EN.write("0 ")
                # 		outputfile_INV.write("0 ")
                counter += 1
        # print(counter)
        # print("\n")
        for compensate in range(0, 228 - counter):
            outputfile_MEMORY_data0.write(str(0) + ",")
            outputfile_MEMORY_data0_2.write(str(0) + ",")
            # if compensate!=227-counter:
            # 	outputfile_MEMORY_data0.write("\n")
            # 	outputfile_MEMORY_data0_2.write("\n")
            outputfile_MEMORY_data1.write(str(0) + ",")
            outputfile_MEMORY_data1_2.write(str(0) + ",")
            # if compensate!=227-counter:
            # 	outputfile_MEMORY_data1.write("\n")
            # 	outputfile_MEMORY_data1_2.write("\n")
            outputfile_MEMORY_data2.write(str(0) + ",")
            outputfile_MEMORY_data2_2.write(str(0) + ",")
            # if compensate!=227-counter:
            # 	outputfile_MEMORY_data2.write("\n")
            # 	outputfile_MEMORY_data2_2.write("\n")


main()
