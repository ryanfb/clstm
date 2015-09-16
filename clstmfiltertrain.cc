#include "pstring.h"
#include "clstm.h"
#include "clstmhl.h"
#include <assert.h>
#include <iostream>
#include <vector>
#include <memory>
#include <math.h>
#include <Eigen/Dense>
#include <sstream>
#include <fstream>
#include <iostream>
#include <set>

#include "multidim.h"
#include "pymulti.h"
#include "extras.h"

using namespace Eigen;
using namespace ocropus;
using namespace pymulti;
using std::vector;
using std::map;
using std::make_pair;
using std::shared_ptr;
using std::unique_ptr;
using std::cout;
using std::ifstream;
using std::set;
using std::to_string;
using std_string = std::string;
using std_wstring = std::wstring;
#define string std_string
#define wstring std_wstring

struct Sample {
  wstring in, out;
};

void read_samples(vector<Sample> &samples, const string &fname) {
  ifstream stream(fname);
  string line;
  wstring in, out;
  ;
  samples.clear();
  while (getline(stream, line)) {
    // skip blank lines and lines starting with a comment
    if (line.substr(0, 1) == "#") continue;
    if (line.size() == 0) continue;
    int where = line.find("\t");
    if (where < 0) THROW("no tab found in input line");
    in = utf8_to_utf32(line.substr(0, where));
    out = utf8_to_utf32(line.substr(where + 1));
    if (in.size() == 0) continue;
    if (out.size() == 0) continue;
    samples.push_back(Sample{in, out});
  }
}

void get_codec(vector<int> &codec, vector<Sample> &samples,
               wstring Sample::*p) {
  set<int> codes;
  codes.insert(0);
  for (auto e : samples) {
    for (auto c : e.*p) codes.insert(int(c));
  }
  for (auto c : codes) codec.push_back(c);
  for (int i = 1; i < codec.size(); i++) assert(codec[i] > codec[i - 1]);
}

int main1(int argc, char **argv) {
  if (argc < 2 || argc > 3) THROW("... training [testing]");
  vector<Sample> samples, test_samples;
  read_samples(samples, argv[1]);
  if (argc > 2) read_samples(test_samples, argv[2]);
  print("got", samples.size(), "inputs,", test_samples.size(), "tests");

  vector<int> icodec, codec;
  get_codec(icodec, samples, &Sample::in);
  get_codec(codec, samples, &Sample::out);

  CLSTMText clstm;
  clstm.createBidi(icodec, codec, getienv("nhidden", 100));
  clstm.setLearningRate(getdenv("rate", 1e-4), getdenv("momentum", 0.9));
  clstm.net->info("");

  int ntrain = getienv("ntrain", 10000000);
  int save_every = getienv("save_every", 10000);
  string save_name = getsenv("save_name", "_filter");
  int report_every = getienv("report_every", 100);
  int test_every = getienv("test_every", 10000);

  // Command to execute after testing the networks performance.
  string after_test = getsenv("after_test", "");

  double best_error = 1e38;
  double test_error = 9999.0;
  for (int trial = 0; trial < ntrain; trial++) {
    int sample = irandom() % samples.size();
    if (trial > 0 && test_samples.size() > 0 && test_every > 0 &&
        trial % test_every == 0) {
      double errors = 0.0;
      double count = 0.0;
      for (int test = 0; test < test_samples.size(); test++) {
        wstring gt = test_samples[test].out;
        wstring pred = clstm.predict(test_samples[test].in);
        count += gt.size();
        errors += levenshtein(pred, gt);
      }
      test_error = errors / count;
      print("ERROR", trial, test_error, "   ", errors, count);
      if (save_every == 0 && test_error < best_error) {
        best_error = test_error;
        string fname = save_name + ".clstm";
        print("saving best performing network so far", fname,
              "error rate: ", best_error);
        clstm.save(fname);
      }
      if (after_test != "") system(after_test.c_str());
    }
    if (trial > 0 && save_every > 0 && trial % save_every == 0) {
      string fname = save_name + "-" + to_string(trial) + ".clstm";
      clstm.save(fname);
    }
    wstring pred = clstm.train(samples[sample].in, samples[sample].out);
    if (trial % report_every == 0) {
      print("trial", trial);
      print("INP", samples[sample].in);
      print("TRU", samples[sample].out);
      print("ALN", clstm.aligned_utf8());
      print("OUT", pred);
    }
  }

  return 0;
}

int main(int argc, char **argv) {
#ifdef NOEXCEPTION
  return main1(argc, argv);
#else
  try {
    return main1(argc, argv);
  } catch (const char *message) {
    cerr << "FATAL: " << message << endl;
  }
#endif
}
