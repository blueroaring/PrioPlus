# NASA-NJU/NS3-RDMA

This project is an RDMA network simulator based on NS3 developed for NJU NASA. The project is still in a closed development phase and has not been open-sourced yet. Please protect the source code.

## Getting started

The project is mainly divided into two parts: `src/dcb` and `src/json-util`.

- The `src/dcb` directory contains RDMA simulation code.
- The `src/json-util` directory includes utility code for experimental configuration and result output using JSON.

The `src/json-util` module relies on the `boost::json` library and requires installing `boost` version 1.75 or higher. If there is an existing version of the boost library in the system that is currently in use and it's inconvenient to configure the PATH, you can directly specify the path to the boost library during the `ns3 configure` step (optional).

### Configure

Configure ns3 using the following command:

```bash
./ns3 configure -d debug --enable-examples --enable-tests --disable-gtk --cmake-prefix-path '/path/to/your/boost/' --enable-mtp
```

When running large experiments, it is recommended to configure with `-d release` or `-d optimized`

### Run demos

You can run all the demos by executing `bash config/demos/run-all.sh` in the project's root directory. Generally, all the demos will be finished in 2 mins under debug mode.