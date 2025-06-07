# lz

Compress input file using Lempel-Ziv algorithms.

These are the currently implemented algorithms (more to be added):
- LZ77
- LZ78
- LZW

## Usage

Run `make` to build the `lz` binary:

```sh
make
```

### Compress a file

To compress a file and write the result to stdout:

```sh
./lz input.txt
```

To save the result to a file:

```sh
./lz input.txt output.lz
```

### Decompress a file

To decompress a file and write the result to stdout:

```sh
./lz -d output.lz
```

To save the result to a file:

```sh
./lz -d output.lz input2.txt
```

### Change the algorithm

To change the algorithm for compression/decompression:

```sh
./lz -a LZ78 input.txt output.lz
./lz -d -a LZ78 output.lz input2.txt
```

## License

This project is licensed under the MIT License. See [LICENSE](./LICENSE) for more details.
