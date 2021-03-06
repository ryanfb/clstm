# clstm

[![Join the chat at https://gitter.im/tmbdev/clstm](https://badges.gitter.im/Join%20Chat.svg)](https://gitter.im/tmbdev/clstm?utm_source=badge&utm_medium=badge&utm_campaign=pr-badge&utm_content=badge)

To build a standalone C library, run

    scons
    sudo scons install
    
Prerequisites:

 - scons, Eigen
 - protocol buffer library and compiler
 - HDF5 libraries and C++, Python bindings (optional, for HDF5 I/O)
 - ZMQ libraries and C++, Python bindings (optional, for display)

On Ubuntu, this means:

    sudo apt-get install libeigen3-dev \
    hdf5-helpers libhdf5-8 libhdf5-cpp-8 libhdf5-dev python-h5py \
    libprotobuf-dev libprotobuf9 protobuf-compiler \                  
    libzmq3-dev libzmq3 libzmqpp-dev libzmqpp3 libpng12-dev
    
There are a bunch of options:

 - `debug=1` build with debugging options, no optimization
 - `display=1` build with display support for debugging (requires ZMQ, Python)
 - `prefix=...` install under a different prefix (untested)
 - `eigen=...` where to look for Eigen include files (should contain `Eigen/Eigen`)
 - `hdf5lib=hdf5` what HDF5 library to use; enables HDF5 command line programs (may need `hdf5_serial` in some environments)

After building the executables, you can run two simple test runs as follows:

 - `run-cmu` will train an English-to-IPA LSTM
 - `run-uw3-500` will download a small OCR training/test set and train an OCR LSTM

To build the Python extension, run

    python setup.py build
    sudo python setup.py install

# Documentation / Examples

You can find some documentation and examples in the form of iPython notebooks in the `misc` directory
(these are version 3 notebooks and won't open in older versions).

You can view these notebooks online here:
http://nbviewer.ipython.org/github/tmbdev/clstm/tree/master/misc/

# C++ API

The `clstm` library operates on the Sequence type as its fundamental
data type, representing variable length sequences of fixed length vectors.
Internally, this is represented as an STL vector of Eigen dynamic vectors

    typedef stl::vector<Eigen::VectorXf> Sequence;

NB: This will be changed to an Eigen::Tensor

Networks are built from objects implementing the `INetwork` interface.
The `INetwork` interface contains:

    struct INetwork {
        Sequence inputs, d_inputs;      // input sequence, input deltas
        Sequence outputs, d_outputs;    // output sequence, output deltas
        void forward();                 // propagate inputs to outputs
        void backward();                // propagate d_outputs to d_inputs
        void update();                  // update weights from the last backward() step
        void setLearningRate(Float,Float); // set learning rates
        ...
    };

Network structures can be hierarchical and there are some network 
implementations whose purpose it is to combine other networks into more
complex structures.

    struct INetwork {
        ...
        vector<shared_ptr<INetwork>> sub;
        void add(shared_ptr<INetwork> net);
        ...
    };

At its lowest level, layers are created by:

 - create an instance of the layer with `make_layer`
 - set any parameters (including `ninput` and `noutput`) as
   attributes
 - add any sublayers to the `sub` vector
 - call `initialize()`

There are three different functions for constructing layers and networks:

 - `make_layer(kind)` looks up the constructor and gives you an uninitialized layer
 - `layer(kind,ninput,noutput,args,sub)` performs all initialization steps in sequence
 - `make_net(kind,args)` initializes a whole collection of layers at once
 - `make_net_init(kind,params)` is like `make_net`, but parameters are given in string form

The `layer(kind,ninput,noutput,args,sub)` function will perform 
these steps in sequence.

Layers and networks are usually passed around as `shared_ptr<INetwork>`;
there is a typedef of this calling it `Network`.

This can be used to construct network architectures in C++ pretty
easily. For example, the following creates a network that stacks
a softmax output layer on top of a standard LSTM layer:

    Network net = layer("Stacked", ninput, noutput, {}, {
        layer("LSTM", ninput, nhidden,{},{}),
        layer("SoftmaxLayer", nhidden, noutput,{},{})
    });

Note that you need to make sure that the number of input and
output units are consistent between layers.

In addition to these basic functions, there is also a small implementation
of CTC alignment.

The C++ code roughly follows the lstm.py implementation from the Python
version of OCRopus. Gradients have been verified for the core LSTM
implementation, although there may be still be bugs in other parts of
the code.

There is also a small multidimensional array class in `multidim.h`; that
isn't used in the core LSTM implementation, but it is used in debugging
and testing code, for plotting, and for HDF5 input/output. Unlike Eigen,
it uses standard C/C++ row major element order, as libraries like
HDF5 expect. (NB: This will be replaced with Eigen::Tensor.)

LSTM models are stored in protocol buffer format (`clstm.proto`), 
although adding new formats is easy. There is an older HDF5-based 
storage format.

# Python API

The `clstm.i` file implements a simple Python interface to clstm, plus
a wrapper that makes an INetwork mostly a replacement for the lstm.py
implementation from ocropy.

# Comand Line Drivers

There are several command line drivers:

  - `clstmfiltertrain training-data test-data` learns text filters;
    - input files consiste of lines of the form "input<tab>output<nl>"
  - `clstmfilter` applies learned text filters
  - `clstmocrtrain training-images test-images` learns OCR (or image-to-text) transformations;
    - input files are lists of text line images; the corresponding UTF-8 ground truth is expected in the corresponding `.gt.txt` file
  - `clstmocr` applies learned OCR models
 
 In addition, you get the following HDF5-based commands:

  - clstmseq learns sequence-to-sequence mappings
  - clstmctc learns sequence-to-string mappings using CTC alignment
  - clstmtext learns string-to-string transformations

Note that most parameters are passed through the environment:

    lrate=3e-5 clstmctc uw3-dew.h5
    
See the notebooks in the `misc/` subdirectory for documentation on the parameters and examples of usage.

(You can find all parameters via `grep 'get.env' *.cc`.)

# TODO / UPCOMING

  - Lua and Torch bindings
  - more recurrent network types
  - replacement of mdarray with Eigen Tensors
  - 2D LSTM support
