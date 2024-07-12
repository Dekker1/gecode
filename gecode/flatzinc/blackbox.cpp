#include <dlfcn.h>
#include <gecode/int.hh>

#include <gecode/flatzinc/blackbox.hh>

namespace Gecode {
namespace FlatZinc {

PluginContainer::PluginContainer(const std::string &dll_path) : _library(0) {
  // We will make the path a variable that is passed in, in future
  std::cerr << "loading library " << dll_path << std::endl;
  void *_library;
  _library = dlopen(dll_path.c_str(), RTLD_LAZY);
  if (!_library)
    throw "error loading library\n" + std::string(dlerror());
  dlerror(); // reset errors;
  std::cerr << "loaded library " << dll_path << " succesfully" << std::endl;

  // load create function
  blackboxFunctionPtr =
      (Gecode::FlatZinc::FunctionPtr)dlsym(_library, "run_function");
  if (!blackboxFunctionPtr)
    std::cerr << "Couldn't load function: run_function" << std::endl;
  // throw std::string(dlerror());
};

PluginContainer::~PluginContainer() {
  if (_library)
    dlclose(_library);
};

void PluginContainer::runBlackboxFunction(int *in, int length_in, int *out,
                                          int length_out) {
  (*blackboxFunctionPtr)(in, length_in, out, length_out);
}

void blackbox(Home home, const IntVarArgs &xv, const IntVarArgs &yv,
              const std::string dll_path) {
  ViewArray<Int::IntView> x(home, xv);
  ViewArray<Int::IntView> y(home, yv);

  if (home.failed())
    return;
  PostInfo pi(home);
  GECODE_ES_FAIL(Gecode::FlatZinc::BlackBox::post(home, x, y, dll_path));
}

// The x(x0), and y(y0) initialise the values
// How should I be naming the x0 and y0?
// How should we pass in the dll file path?
BlackBox::BlackBox(Home home, ViewArray<Int::IntView> &x0,
                   ViewArray<Int::IntView> &y0, std::string dll_path)
    : Propagator(home), x(x0), y(y0) {
  bbFunctionContainer = new Gecode::FlatZinc::PluginContainer(dll_path);
  x.subscribe(home, *this, Int::PC_INT_VAL);
}
// Constructor for cloning
// What does calling Propagator(home, p) do ? Is the same as `super` in Java
// Do we actually need to do x(p.x) and y(p.y)? why does it work without them,
// is the super class call initilising them?
BlackBox::BlackBox(Space &home, BlackBox &p)
    : Propagator(home, p), bbFunctionContainer(p.bbFunctionContainer) {
  // How does this bit of code clone the BlackBox object?
  // What does this update function do?
  x.update(home, p.x);
  y.update(home, p.y);
}
// Cost function (defined as low linear)
PropCost BlackBox::cost(const Space &home, const ModEventDelta &med) const {
  return PropCost::binary(PropCost::LO);
};
// Schedule function
void BlackBox::reschedule(Space &home) {
  x.cancel(home, *this, Int::PC_INT_VAL);
  y.cancel(home, *this, Int::PC_INT_VAL);
};

size_t BlackBox::dispose(Space &home) {
  x.cancel(home, *this, Int::PC_INT_VAL);
  (void)Propagator::dispose(home);
  // destroy plugin container
  return sizeof(*this);
};

ExecStatus BlackBox::propagate(Space &home, const ModEventDelta &) {
  if (x.assigned()) {
    std::vector<int> in = std::vector<int>(x.size());
    std::vector<int> out = std::vector<int>(y.size());
    // std::cerr << "gave it: ";
    for (int i = 0; i < in.size(); i++) {
      // std::cerr << x[i].val() << " ";
      in[i] = x[i].val();
    }
    // std::cerr << std::endl;

    bbFunctionContainer->runBlackboxFunction(in.data(), x.size(), out.data(),
                                             y.size());

    // std::cerr << "got back: ";
    for (int i = 0; i < out.size(); i++) {
      // std::cerr << out[i] << " ";
      if (y[i].eq(home, out[i]) == Int::ME_INT_FAILED) {
        return ES_FAILED;
      }
    }

    // std::cerr << std::endl;
    return ES_SUBSUMED_;
  }
  return ES_NOFIX;
};

ExecStatus BlackBox::post(Home home, ViewArray<Int::IntView> &x,
                          ViewArray<Int::IntView> &y,
                          const std::string dll_path) {
  new (home) BlackBox(home, x, y, dll_path);
  return ES_OK;
}

Propagator *BlackBox::copy(Space &home) {
  return new (home) BlackBox(home, *this);
}
} // namespace FlatZinc
} // namespace Gecode
