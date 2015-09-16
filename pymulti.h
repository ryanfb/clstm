// -*- C++ -*-

#ifndef pymulti_
#define pymulti_

#ifndef NODISPLAY
#include <zmqpp/zmqpp.hpp>
#endif
#include <string>
#include <stdarg.h>
#include <iostream>
#include <memory>
#include "multidim.h"

namespace pymulti {
using std::string;
using std::unique_ptr;
using std::shared_ptr;
using std::cout;
using std::cerr;
using std::endl;
using namespace multidim;

inline string stringf(const char *format, ...) {
  static char buf[4096];
  va_list v;
  va_start(v, format);
  vsnprintf(buf, sizeof(buf), format, v);
  va_end(v);
  return string(buf);
}

#ifdef NODISPLAY
struct PyServer {
  void open(const char *where = "tcp://127.0.0.1:9876") {}
  void setMode(int mode) {}
  string eval(string s) { return ""; }
  string eval(string s, const float *a, int na) { return ""; }
  string eval(string s, const float *a, int na, const float *b, int nb) {
    return "";
  }
  string evalf(const char *format, ...) { return ""; }
  void clf() {}
  void subplot(int rows, int cols, int n) {}
  void plot(mdarray<float> &v, string extra = "") {}
  void plot2(mdarray<float> &u, mdarray<float> &v, string extra = "") {}
  void imshow(mdarray<float> &a, string extra = "") {}
  void imshowT(mdarray<float> &a, string extra = "") {}
};
#else
struct PyServer {
  int mode = 0;  // -1=ignore, 0=uninit, 1=output
  zmqpp::context context;
  unique_ptr<zmqpp::socket> socket;
  void open(const char *where = "default") {
    if (string(where) == "none") {
      mode = -1;
      return;
    }
    if (string(where) == "default") {
      where = "tcp://127.0.0.1:9876";
    }
    socket.reset(new zmqpp::socket(context, zmqpp::socket_type::req));
    string addr = getenv("PYSERVER") ? getenv("PYSERVER") : where;
    cerr << "waiting for python server at " << addr << endl;
    socket->connect(addr.c_str());
    mode = 1;
    eval("print 'OK'");
    cerr << "connected" << endl;
    eval("from pylab import *");
    eval("ion()");
  }
  void setMode(int mode) { this->mode = mode; }
  string eval(string s) {
    if (mode < 0)
      return "";
    else if (mode < 1)
      THROW("uninitialized");
    zmqpp::message message;
    message << s;
    socket->send(message);
    socket->receive(message);
    string result;
    message >> result;
    return result;
  }
  string eval(string s, const float *a, int na) {
    if (mode < 0)
      return "";
    else if (mode < 1)
      THROW("uninitialized");
    string cmd;
    zmqpp::message message;
    message << cmd + s;
    message.add_raw((const char *)a, na * sizeof(float));
    socket->send(message);
    socket->receive(message);
    string response;
    message >> response;
    return response;
  }
  string eval(string s, const float *a, int na, const float *b, int nb) {
    if (mode < 0)
      return "";
    else if (mode < 1)
      THROW("uninitialized");
    string cmd;
    zmqpp::message message;
    message << cmd + s;
    message.add_raw((const char *)a, na * sizeof(float));
    message.add_raw((const char *)b, nb * sizeof(float));
    socket->send(message);
    socket->receive(message);
    string response;
    message >> response;
    return response;
  }
  string evalf(const char *format, ...) {
    static char buf[4096];
    va_list v;
    va_start(v, format);
    vsnprintf(buf, sizeof(buf), format, v);
    va_end(v);
    return eval(buf);
  }
  void clf() { eval("clf()"); }
  void subplot(int rows, int cols, int n) {
    eval(stringf("subplot(%d,%d,%d)", rows, cols, n));
  }
  void plot(mdarray<float> &v, string extra = "") {
    if (extra != "") extra = string(",") + extra;
    if (v.rank() != 1) THROW("bad rank");
    eval(stringf("plot(farg(1)%s)", extra.c_str()), v.data, v.size());
  }
  void plot2(mdarray<float> &u, mdarray<float> &v, string extra = "") {
    if (extra != "") extra = string(",") + extra;
    if (u.rank() != 1) THROW("bad rank");
    if (v.rank() != 1) THROW("bad rank");
    eval(stringf("plot(farg(1),farg(2)%s)", extra.c_str()), u.data, u.size(),
         v.data, v.size());
  }
  void imshow(mdarray<float> &a, string extra = "") {
    if (extra != "") extra = string(",") + extra;
    if (a.rank() != 2) THROW("bad rank");
    eval(stringf("imshow(farg2(1,%d,%d)%s)", a.dim(0), a.dim(1), extra.c_str()),
         a.data, a.dim(0) * a.dim(1));
  }
  void imshowT(mdarray<float> &a, string extra = "") {
    if (extra != "") extra = string(",") + extra;
    if (a.rank() != 2) THROW("bad rank");
    eval(stringf("imshow(farg2(1,%d,%d).T%s)", a.dim(0), a.dim(1),
                 extra.c_str()),
         a.data, a.dim(0) * a.dim(1));
  }
};
#endif

inline PyServer *make_PyServer() { return new PyServer(); }
}

#endif
