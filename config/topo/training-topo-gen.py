import sys, argparse, os, json

def main(argv):
    num_server = 4
    gpu_per_server = 8
    gpu_per_tor = 2
    output_name = ""

    help_msg = "training-topo-gen.py -s num_server -g gpu_per_server -gt gpu_per_tor -o output_file_name \n" + \
               "eg: python3 training-topo-gen.py -s 4 -g 8 -t 2 -o training.txt"

    parser = argparse.ArgumentParser()
    parser.add_argument("-s", "--server", help="number of servers", type=int)
    parser.add_argument("-g", "--gpu", help="number of gpus per server", type=int)
    parser.add_argument("-t", "--gpu_tor", help="number of gpus per tor", type=int)
    parser.add_argument("-o", "--output", help="output file name")
    args = parser.parse_args()

    if args.server:
        num_server = args.server
    if args.gpu:
        gpu_per_server = args.gpu
    if args.gpu_tor:
        gpu_per_tor = args.gpu_tor
    if args.output:
        output_name = args.output

    num_gpu = num_server * gpu_per_server
    num_tor = int(num_gpu / gpu_per_tor)
    # node includes gpu, nvswitch (one per server), tor and spine (one for all tor)
    num_node = num_gpu + num_server + num_tor + 1

    # generate the topo into a string, where each line is a link in format "src dst type"
    topo = ""
    nvswitch_begin = num_gpu
    tor_begin = nvswitch_begin + num_server
    spine_id = num_node - 1
    num_link = 0

    # generate the links between gpu and nvswitch with type "nvlink"
    for i in range(num_server):
        for j in range(gpu_per_server):
            topo += str(i*gpu_per_server+j) + " " + str(nvswitch_begin+i) + " nvlink\n"
    num_link += num_gpu

    # generate the links between gpu and tor with type "ethernet1"
    for i in range(num_server):
        for j in range(gpu_per_server):
            topo += str(i*gpu_per_server+j) + " " + str(tor_begin + (i // gpu_per_tor) * gpu_per_server + j) + " ethernet1\n"
    num_link += num_gpu

    # generate the links between tor and spine with type "ethernet2"
    for i in range(num_tor):
        topo += str(tor_begin + i) + " " + str(spine_id) + " ethernet2\n"
    num_link += num_tor

    # open the file
    def open_file(filename):
        try:
            return open(filename, "w")
        except:
            print("Error: cannot open file " + filename)
            sys.exit(1)

    # close the file
    def close_file(file):
        file.close()

    file = open_file(output_name)

    # write the header: num_gpu num_switch num_links
    file.write(str(num_node) + " " + str(num_node - num_gpu) + " " + str(num_link) + "\n")
    # write the second line: id of switches
    for i in range(num_gpu, num_node):
        file.write(str(i) + " ")
    file.write("\n")
    # write the topo
    file.write(topo)

    close_file(file)


if __name__ == "__main__":
    main(sys.argv[1:])
