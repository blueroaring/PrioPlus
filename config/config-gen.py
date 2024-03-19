#!/usr/bin/python3
# -*- coding: UTF-8 -*-

import sys, argparse, os, json

result_prefix = "output/"

# generate a series of config files based on a given config file
def gen_config(scratch_name, config_filename, attributes:list, begins:list, ends:list, step_num, units:list, is_value_floats:list, output_dir):
    output_config_filenames = []

    # read the config
    with open(config_filename, 'r') as cf:
        config = json.load(cf)
        attributes_list = [a.split("::") for a in attributes]
        # step = (end - begin) / (step_num - 1)
        step_list = [(e - b) / (step_num - 1) for b, e in zip(begins, ends)]
        for step, is_float in zip(step_list, is_value_floats):
            assert(is_float or step % 1 == 0)  # check if step for integer value is float

        for i in range(step_num):
            out_filename = config_filename.split("/")[-1][:-5]+"-"

            # find the attribute in the config
            for attribute, attribute_str, begin, step, unit, is_float in zip(attributes_list, attributes , begins, step_list, units, is_value_floats):
                current_config = config
                for a in attribute[:-1]:
                    # the current_config may be a list not a dict
                    if isinstance(current_config, list):
                        current_config = current_config[int(a)]
                    else:
                        current_config = current_config[a]

                value = round(begin + i * step, 5) if is_float else int(begin + i * step)
                if unit is None:
                    current_config[attribute[-1]] = value
                else:
                    current_config[attribute[-1]] = str(value) + unit

                out_filename += attribute_str + "-" + str(value) + "&"

            # also modify the output file name
            # out_filename = config_filename (without dir and .json) + "-" + attribute + "-" + value
            out_filename = out_filename[:-1]
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
            array_str += "\"" + cfn + "\" "
        array_str = array_str[:-1] + ")\n"

        loop_str = "for((i=0;i<" + str(len(output_config_filenames)) + ";i+=1)) do \n{\n"
        loop_str += "    ./ns3 run \"" + scratch_name + " config/${configs[$i]}\" \n"
        loop_str += "}& \ndone"

        shf.writelines(["./ns3\n", array_str, loop_str])

def str2num(s):
    try:
        return int(s)
    except ValueError:
        return float(s)

def str2unit(s):
    if s == "":
        return None
    else:
        return s

def main(argv):
    scratch_name = ""
    config_filename = ""
    attribute = []
    begin = []
    end = []
    step_num = 0
    unit = []
    output_dir = ""
    is_value_float = []

    help_msg = "config-gen.py -n scratch_name -c config_file -a attribute -b begin -e end -s step -u unit -o output_dir \n" + \
               "eg: python3 config-gen.py -n nr-mimo -c config.json -a runtimeConfig::load::loadFactor -b 0.2 -e 0.9 -s 8 -o test/ \n" + \
               "    python3 config-gen.py -n nr-mimo -c config.json -a runtimeConfig::load::averageSize -b 500000 -e 4000000 -s 8 -o test/ \n" + \
               "    python3 config-gen.py -n nr-mimo -c config.json -a defaultConfig::FifoQueueDisc::MaxSize -b 500000 -e 4000000 -s 8 -u B -o test/"

    parser = argparse.ArgumentParser()
    parser.add_argument("-n", "--scratch", help="scratch name", required=True)
    parser.add_argument("-c", "--config", help="config file name", required=True)
    parser.add_argument("-a", "--attribute", nargs='+', help="attribute name", required=True)
    parser.add_argument("-b", "--begin", nargs='+', help="begin value", required=True)
    parser.add_argument("-e", "--end", nargs='+', help="end value", required=True)
    parser.add_argument("-s", "--step", help="step number", required=True, type=int)
    parser.add_argument("-u", "--unit", nargs='+', help="unit")
    parser.add_argument("-f", "--isFloat", help="is value float")
    parser.add_argument("-o", "--outputdir", help="output dir", required=True)
    args = parser.parse_args()

    # attribute, begin, end, unit, is_value_float should be lists, even if they only have one element
    scratch_name = args.scratch
    config_filename = args.config
   
    attribute = args.attribute if isinstance(args.attribute, list) else [args.attribute]
    begin = [str2num(b) for b in args.begin] if isinstance(args.begin, list) else [str2num(args.begin)]
    end = [str2num(e) for e in args.end] if isinstance(args.end, list) else [str2num(args.end)]
    
    step_num = int(args.step)
    
    if args.unit is None:
        unit = [None] * len(attribute)
    else:
        unit = [str2unit(u) for u in args.unit] if isinstance(args.unit, list) else [str2unit(args.unit)]

    if args.isFloat is None:
        is_value_float = [False] * len(attribute)
    else:
        is_value_float = [bool(i) for i in args.isFloat] if isinstance(args.isFloat, list) else [bool(f) for f in args.isFloat]
    
    output_dir = args.outputdir

    try:
        os.makedirs(output_dir)
    except:
        pass
    try:
        os.makedirs("../" + result_prefix + output_dir)
    except:
        pass

    # if both inputs are integers, we treat all the values as integers
    for i in range(len(begin)):
        for v in [begin[i], end[i]]:
            if v % 1 != 0 or is_value_float[i]:
                is_value_float[i] = True

    # determine if the config_filename is a file or a dir
    if os.path.isfile(config_filename):
        # generate a series of config files based on the given config file
        gen_config(scratch_name, config_filename, attribute, begin, end, step_num, unit, is_value_float, output_dir)
    else:
        # generate a series of config files based on all the config files in the given dir
        config_list = os.listdir(config_filename)
        config_list = [cf for cf in config_list if cf != "run.sh"]
        for cf in config_list:
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
            for cf in config_list:
                array_str += output_dir + cf[:-5] + " "
            array_str = array_str[:-1] + ")\n"

            loop_str = "for((i=0;i<" + str(len(config_list)) + ";i+=1)) do \n{\n"
            loop_str += "    bash config/${configs[$i]}/run.sh \n"
            loop_str += "}& \ndone"

            shf.writelines(["./ns3\n", array_str, loop_str])

if __name__ == "__main__":
    main(sys.argv[1:])
