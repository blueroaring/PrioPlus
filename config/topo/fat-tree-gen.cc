/*
 * This is a fat-tree topology generator.
 * To compile it, run `g++ fat-tree-gen.cc -o fat-tree-gen`
 * Until now, this generator only supports single link type, i.e., 0.
 */

#include <assert.h>
#include <fstream>
#include <iostream>
using namespace std;

int core = 0, spine = 0, leaf = 0, host = 0;
int cbw, cdl, sbw, sdl, lbw, ldl;
double hdl;
int leaf_per_pod = 4, spine_per_pod = 2;
int pod_num = 0;

int k;
int bw, dl;

inline int
get_host_index(int host_number)
{
    return host_number;
}

inline int
get_leaf_index(int leaf_number)
{
    return host * leaf + leaf_number; // host * leaf is the total num of host
}

inline int
get_spine_index(int spine_number)
{
    return host * leaf + leaf + spine_number;
}

inline int
get_core_index(int core_number)
{
    return host * leaf + leaf + spine + core_number;
}

int
main()
{
	cout<<"k for fat-tree: ";
	cout.flush(); cin >> k;
	
    pod_num = k;
    leaf_per_pod = k / 2;
    spine_per_pod = k / 2;

    leaf = pod_num * leaf_per_pod;
    spine = pod_num * spine_per_pod;
    core = (pod_num / 2) * (pod_num / 2);
    host = k / 2;

    string file;
    cout << "The output file name: ";
    cout.flush();
    cin >> file;

    ofstream fout(file.c_str());
    int host_num = host * leaf;
    int sw_num = leaf + spine + core;
    int nodes = host_num + leaf + spine + core;
    int host_per_rack = leaf_per_pod * host;
    int tor_num = leaf;
    int core_num = core;
    int link_num = core_num * pod_num + (spine_per_pod * leaf_per_pod) * pod_num + host * leaf;
    fout << nodes << " " << sw_num << " " << link_num
         << endl; // all nodes, all hosts, hosts per rack/pod

    // switch's id
    int first_id = host_num;
    for (int i = 0; i < sw_num; i++)
    {
        fout << first_id++ << " ";
    }
    fout << endl;

    // host->leaf
    for (int i = 0; i < host_num; ++i)
    {
        int s = get_host_index(i);
        int t = get_leaf_index(i / host);
        fout << s << " " << t << " " << 0 << endl;
    }

    // leaf->spine.
    for (int i = 0; i < leaf; ++i)
    {
        int leaf_pod = i / leaf_per_pod;
        int start_index = leaf_pod * spine_per_pod;

        for (int j = start_index; j < start_index + spine_per_pod; ++j)
        {
            int s = get_leaf_index(i);
            int t = get_spine_index(j);
            fout << s << " " << t << " " << 0 << endl;
        }
    }

    // spine->core
    // spine-per-pod is the #class of core
    for (int i = 0; i < spine; ++i)
    {
        int spine_id_in_pod = i % spine_per_pod;   // 0 1
        int core_per_class = core / spine_per_pod; // 8 / 2 = 4
        int start_index = core_per_class * spine_id_in_pod;
        for (int j = start_index; j < start_index + core_per_class; ++j)
        {
            int s = get_spine_index(i);
            int t = get_core_index(j);
            fout << s << " " << t << " " << 0 << endl;
        }
    }

    fout.close();
    return 0;
}
