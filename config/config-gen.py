#!/usr/bin/python3
# -*- coding: UTF-8 -*-

import sys, getopt, os, json

result_prefix = "output/"

# generate a series of config files based on a given config file
def gen_config(scratch_name, config_filename, attribute, begin, end, step_num, unit, is_value_float, output_dir):
    output_config_filenames = []

    # read the config
    with open(config_filename, 'r') as cf:
        config = json.load(cf)
        attributes = attribute.split("::")
        step = (end - begin) / (step_num - 1)
        assert(is_value_float or step % 1 == 0)  # check if step for integer value is float

        for i in range(step_num):
            current_config = config
            for a in attributes[:-1]:
                current_config = current_config[a]
            
            value = round(begin + i * step, 3) if is_value_float else int(begin + i * step)
            if unit is None:
                current_config[attributes[-1]] = value
            else:
                current_config[attributes[-1]] = str(value) + unit
            
            # also modify the output file name
            # out_filename = config_filename (without dir and .json) + "-" + attribute + "-" + value
            out_filename = config_filename.split("/")[-1][:-5]+"-"+attribute+"-"+str(value)
            config["outputFile"]["resultFile"] = result_prefix + output_dir + out_filename + ".json"

            # write out generated json
            # print(output_dir + out_filename + ".json")
            with open(output_dir + out_filename + ".json", 'w') as of:
                output_config_filenames.append(of.name)
                json.dump(config, of, indent=4)

    # write out the .sh script
    with open(output_dir + "run.sh", "w") as shf:
        array_str = "configs=("
        for cfn in output_config_filenames:
            array_str += cfn + " "
        array_str = array_str[:-1] + ")\n"

        loop_str = "for((i=0;i<" + str(len(output_config_filenames)) + ";i+=1)) do \n{\n"
        loop_str += "    ./ns3 run \"" + scratch_name + " config/${configs[$i]}\" \n"
        loop_str += "}& \ndone"

        shf.writelines(["./ns3\n", array_str, loop_str])

def main(argv):
    scratch_name = ""
    config_filename = ""
    attribute = ""
    begin = 0
    end = 0
    step_num = 0
    unit = None
    output_dir = ""
    is_value_float = False

    help_msg = "config-gen.py -n scratch_name -c config_file -a attribute -b begin -e end -s step -u unit -o output_dir \n" + \
               "eg: python3 config-gen.py -n nr-mimo -c config.json -a runtimeConfig::load::loadFactor -b 0.2 -e 0.9 -s 8 -o test/ \n" + \
               "    python3 config-gen.py -n nr-mimo -c config.json -a runtimeConfig::load::averageSize -b 500000 -e 4000000 -s 8 -o test/ \n" + \
               "    python3 config-gen.py -n nr-mimo -c config.json -a defaultConfig::FifoQueueDisc::MaxSize -b 500000 -e 4000000 -s 8 -u B -o test/"
    try:
        opts, args = getopt.getopt(argv,"hn:c:a:b:e:s:u:o:f:",["scratch=", "config=", "attribute=", "begin=", "end=", "step=", "unit=", "outputdir=", "isFloat="])
    except getopt.GetoptError:
        print(help_msg)
        sys.exit(2)

    for opt, arg in opts:
        if opt == '-h':
            print(help_msg)
            sys.exit()
        elif opt in ("-n", "--scratch"):
            scratch_name = arg
        elif opt in ("-c", "--config"):
            config_filename = arg
        elif opt in ("-a", "--attribute"):
            attribute = arg
        elif opt in ("-b", "--begin"):
            begin = float(arg)
        elif opt in ("-e", "--end"):
            end = float(arg)
        elif opt in ("-s", "--step"):
            step_num = int(arg)
        elif opt in ("-u", "--unit"):
            unit = arg
        elif opt in ("-f", "--isFloat"):
            is_value_float = bool(arg)
        elif opt in ("-o", "--outputdir"):
            output_dir = arg
            try:
                os.makedirs(output_dir)
            except:
                pass
            try:
                os.makedirs("../" + result_prefix + output_dir)
            except:
                pass
    
    # if both inputs are integers, we treat all the values as integers
    for v in [begin, end]:
        if v % 1 != 0 or is_value_float:
            is_value_float = True

    # determine if the config_filename is a file or a dir
    if os.path.isfile(config_filename):
        # generate a series of config files based on the given config file
        gen_config(scratch_name, config_filename, attribute, begin, end, step_num, unit, is_value_float, output_dir)
    else:
        # generate a series of config files based on all the config files in the given dir
        for cf in os.listdir(config_filename):
            # make output dir and resulet dir for each config file
            local_output_dir = output_dir + cf[:-5] + "/"
            try:
                os.makedirs(local_output_dir)
            except:
                pass
            try:
                os.makedirs("../" + result_prefix + local_output_dir)
            except:
                pass
            gen_config(scratch_name, config_filename + cf, attribute, begin, end, step_num, unit, is_value_float, local_output_dir)
        # write out the .sh script for running all the generated configs
        with open(output_dir + "run.sh", "w") as shf:
            array_str = "configs=("
            for cf in os.listdir(config_filename):
                array_str += output_dir + cf[:-5] + " "
            array_str = array_str[:-1] + ")\n"

            loop_str = "for((i=0;i<" + str(len(os.listdir(config_filename))) + ";i+=1)) do \n{\n"
            loop_str += "    bash config/${configs[$i]}/run.sh \n"
            loop_str += "}& \ndone"

            shf.writelines(["./ns3\n", array_str, loop_str])

if __name__ == "__main__":
    main(sys.argv[1:])