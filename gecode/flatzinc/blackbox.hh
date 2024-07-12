
#include <gecode/flatzinc.hh>

#include <string>
#include <type_traits>

namespace Gecode {
namespace FlatZinc {
using FunctionPtr = std::add_pointer<void(int *, int, int *, int)>::type;
class PluginContainer {
public:
  PluginContainer(const std::string &path);
  ~PluginContainer();
  void runBlackboxFunction(int *in, int length_in, int *out, int length_out);

private:
  FunctionPtr blackboxFunctionPtr;
  void *_library;
};

class BlackBox : public Propagator {
protected:
  // Array of views
  ViewArray<Int::IntView> x;
  /// Array of views
  ViewArray<Int::IntView> y;
  PluginContainer *bbFunctionContainer;
  /// Constructor for cloning \a p
  BlackBox(Space &home, BlackBox &p);

public:
  /// Constructor for creation
  BlackBox(Home home, ViewArray<Int::IntView> &x0, ViewArray<Int::IntView> &y0,
           std::string dll_path);
  /// Cost function (defined as low linear)
  virtual PropCost cost(const Space &home, const ModEventDelta &med) const;
  /// Schedule function
  virtual void reschedule(Space &home);
  /// Delete propagator and return its size
  virtual size_t dispose(Space &home);

  virtual ExecStatus propagate(Space &home, const ModEventDelta &);

  virtual Propagator *copy(Space &home);

  static ExecStatus post(Home home, ViewArray<Int::IntView> &x,
                         ViewArray<Int::IntView> &y, std::string dll_path);
};
void blackbox(Home home, const IntVarArgs &xv, const IntVarArgs &yv,
              const std::string dll_path);

} // namespace FlatZinc
} // namespace Gecode
