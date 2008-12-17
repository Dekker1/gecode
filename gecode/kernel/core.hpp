/* -*- mode: C++; c-basic-offset: 2; indent-tabs-mode: nil -*- */
/*
 *  Main authors:
 *     Christian Schulte <schulte@gecode.org>
 *     Guido Tack <tack@gecode.org>
 *     Mikael Lagerkvist <lagerkvist@gecode.org>
 *
 *  Copyright:
 *     Christian Schulte, 2002
 *     Guido Tack, 2003
 *     Mikael Lagerkvist, 2006
 *
 *  Bugfixes provided by:
 *     Alexander Samoilov <alexander_samoilov@yahoo.com>
 *
 *  Last modified:
 *     $Date$ by $Author$
 *     $Revision$
 *
 *  This file is part of Gecode, the generic constraint
 *  development environment:
 *     http://www.gecode.org
 *
 *  Permission is hereby granted, free of charge, to any person obtaining
 *  a copy of this software and associated documentation files (the
 *  "Software"), to deal in the Software without restriction, including
 *  without limitation the rights to use, copy, modify, merge, publish,
 *  distribute, sublicense, and/or sell copies of the Software, and to
 *  permit persons to whom the Software is furnished to do so, subject to
 *  the following conditions:
 *
 *  The above copyright notice and this permission notice shall be
 *  included in all copies or substantial portions of the Software.
 *
 *  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 *  EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 *  MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 *  NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE
 *  LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION
 *  OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION
 *  WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 *
 */

namespace Gecode {

  class Space;

  /**
   * \defgroup FuncSupportShared Support for copied and shared objects and handles
   *
   * Copied handles provide access to objects that are updated when a space
   * is copied, used by entities inside spaces. 
   * An update creates a single copy per space during updating.
   *
   * Shared handles provide access to reference-counted objects. In
   * particular, they support updates with and without sharing. 
   * An update with sharing just updates the handle, while a non-shared
   * update creates a single copy per space.
   *
   * \ingroup FuncSupport
   */

  /**
   * \brief The copied handle
   *
   * A copied handle provides access to an object that lives in a space, and
   * is used by entities inside the space. The handle has an update mechanism
   * that makes sure that a single copy of the object is created when the
   * space is copied.
   *
   * This is the base class that all copied handles must inherit from.
   *
   * \ingroup FuncSupportShared
   */
  class CopiedHandle {
  public:
    /**
     * \brief The copied object
     *
     * Copied objects must inherit from this base class. 
     *
     * \ingroup FuncSupportShared
     */
    class Object {
      friend class Space;
      friend class CopiedHandle;
    private:
      /// The next object collected during copying
      Object* next;
      /// The forwarding pointer
      Object* fwd;
    public:
      /// Initialize
      Object(void);
      /// Return fresh copy for update
      virtual Object* copy(void) const = 0;
      /// Delete object
      virtual ~Object(void);
      /// Allocate memory from space
      static void* operator new(size_t s, Space& s);
      /// Free memory
      static void operator delete(void*, size_t);
      /// No-op (for exceptions)
      static void operator delete(void*, Space&);
    private:
      static void* operator new(size_t s);
    };
  private:
    /// The shared object
    Object* o;
  public:
    /// Create shared handle with no object pointing to
    CopiedHandle(void);
    /// Create shared handle that points to shared object \a so
    CopiedHandle(Object* so);
    /// Copy constructor maintaining reference count
    CopiedHandle(const CopiedHandle& sh);
    /// Assignment operator mainitaining reference count
    CopiedHandle& operator =(const CopiedHandle& sh);
    /// Updating during cloning
    void update(Space& home, bool share, CopiedHandle& sh);
    /// Deallocate object
    void dispose(Space& home);
  protected:
    /// Access to the shared object
    Object* object(void) const;
    /// Modify shared object
    void object(Object* n);
  };
  
  /**
   * \brief The shared handle
   *
   * A shared handle provides access to an object that lives outside a space, 
   * and is shared between entities that possibly reside inside different 
   * spaces. The handle has an update mechanism that supports updates with and
   * without sharing. An update without sharing makes sure that a
   * single copy of the object is created when the space is copied.
   *
   * This is the base class that all shared handles must inherit from.
   *
   * \ingroup FuncSupportShared
   */
  class SharedHandle : public CopiedHandle {
  public:
    /**
     * \brief The shared object
     *
     * Shared objects must inherit from this base class. 
     *
     * \ingroup FuncSupportShared
     */
    class Object : public CopiedHandle::Object {
      friend class Space;
      friend class SharedHandle;
    private:
      /// The counter used for reference counting
      unsigned int use_cnt;
    public:
      /// Initialize
      Object(void);
      /// Delete shared object
      virtual ~Object(void);
      /// Allocate memory from heap
      static void* operator new(size_t s);
      /// Free memory allocated from heap
      static void  operator delete(void* p);
    };
  private:
    /// Subscribe handle to object
    void subscribe(void);
    /// Cancel subscription of handle to object
    void cancel(void);
  public:
    /// Create shared handle with no object pointing to
    SharedHandle(void);
    /// Create shared handle that points to shared object \a so
    SharedHandle(Object* so);
    /// Copy constructor maintaining reference count
    SharedHandle(const SharedHandle& sh);
    /// Assignment operator mainitaining reference count
    SharedHandle& operator =(const SharedHandle& sh);
    /// Updating during cloning
    void update(Space& home, bool share, SharedHandle& sh);
    /// Destructor that maintains reference count
    ~SharedHandle(void);
  protected:
    /// Access to the shared object
    Object* object(void) const;
    /// Modify shared object
    void object(Object* n);
  };


  /**
   * \defgroup TaskVarMEPC Generic modification events and propagation conditions
   *
   * Predefined modification events must be taken into account
   * by variable types.
   * \ingroup TaskVar
   */
  //@{
  /// Type for modification events
  typedef int ModEvent;

  /// Generic modification event: failed variable
  const ModEvent ME_GEN_FAILED   = -1;
  /// Generic modification event: no modification
  const ModEvent ME_GEN_NONE     =  0;
  /// Generic modification event: variable is assigned a value
  const ModEvent ME_GEN_ASSIGNED =  1;

  /// Type for propagation conditions
  typedef int PropCond;
  /// Propagation condition to be ignored (convenience)
  const PropCond PC_GEN_NONE     = -1;
  /// Propagation condition for an assigned variable
  const PropCond PC_GEN_ASSIGNED = 0;
  //@}

  /**
   * \brief Modification event deltas
   *
   * Modification event deltas are used by propagators. A
   * propagator stores a modification event for each variable type.
   * They can be accessed through a variable or a view from a given
   * propagator. They can be constructed from a given modevent by
   * a variable or view.
   * \ingroup TaskActor
   */
  typedef int ModEventDelta;

}

#include <gecode/kernel/var-type.hpp>

namespace Gecode {

  /// Configuration class for variable implementations without index structure
  class NoIdxVarImpConf {
  public:
    /// Index for update
    static const int idx_c = -1;
    /// Index for disposal
    static const int idx_d = -1;
    /// Maximal propagation condition
    static const PropCond pc_max = PC_GEN_ASSIGNED;
    /// Freely available bits
    static const int free_bits = 0;
    /// Start of bits for modification event delta
    static const int med_fst = 0;
    /// End of bits for modification event delta
    static const int med_lst = 0;
    /// Bitmask for modification event delta
    static const int med_mask = 0;
    /// Combine modification events \a me1 and \a me2
    static Gecode::ModEvent me_combine(ModEvent me1, ModEvent me2);
    /// Update modification even delta \a med by \a me, return true on change
    static bool med_update(ModEventDelta& med, ModEvent me);
    /// Variable type identifier for reflection
    static GECODE_KERNEL_EXPORT const Support::Symbol vti;
  };

  forceinline ModEvent
  NoIdxVarImpConf::me_combine(ModEvent, ModEvent) {
    GECODE_NEVER; return 0;
  }
  forceinline bool
  NoIdxVarImpConf::med_update(ModEventDelta&, ModEvent) {
    GECODE_NEVER; return false;
  }


  /*
   * These are the classes of interest
   *
   */
  class ActorLink;
  class Actor;
  class Propagator;
  class Advisor;
  template <class A> class Council;
  template <class A> class Advisors;
  template <class VIC> class VarImp;


  /*
   * Variable implementations
   *
   */

  /**
   * \brief Base-class for variable implementations
   *
   * Serves as base-class that can be used without having to know any
   * template arguments.
   * \ingroup TaskVar
   */
  class VarImpBase {};

  /**
   * \brief Base class for %Variable type disposer
   *
   * Controls disposal of variable implementations.
   * \ingroup TaskVar
   */
  class GECODE_VTABLE_EXPORT VarDisposerBase {
  public:
    /// Dispose list of variable implementations starting at \a x
    GECODE_KERNEL_EXPORT virtual void dispose(Space& home, VarImpBase* x);
    /// Destructor (not used)
    GECODE_KERNEL_EXPORT virtual ~VarDisposerBase(void);
  };

  /**
   * \brief %Variable type disposer
   *
   * Controls disposal of variables.
   * \ingroup TaskVar
   */
  template <class VarType>
  class VarDisposer : public VarDisposerBase {
  public:
    /// Constructor (registers disposer with kernel)
    VarDisposer(void);
    /// Dispose list of variable implementations starting at \a x
    virtual void dispose(Space& home, VarImpBase* x);
  };

  /// Generic domain change information to be supplied to advisors
  class Delta {
    template <class VIC> friend class VarImp;
  private:
    /// Modification event
    ModEvent me; 
  public:
    /// Return modification event
    ModEvent modevent(void) const;
  };

  /**
   * \brief Base-class for variable implementations
   *
   * Implements variable implementation for variable implementation
   * configuration of type \a VIC.
   * \ingroup TaskVar
   */
  template <class VIC>
  class VarImp : public VarImpBase {
    friend class Space;
    friend class Propagator;
    template <class VarType> friend class VarDisposer;
  private:
    /**
     * \brief Subscribed actors
     *
     * The base pointer of the array of subscribed actors.
     *
     * During cloning, it is reused as the forwarding pointer for the
     * variable. The original value is saved in the copy and restored after
     * cloning.
     *
     * This pointer must be first to avoid padding on 64 bit machines.
     */
    ActorLink** base;

    /// Index for update
    static const int idx_c = VIC::idx_c;
    /// Index for disposal
    static const int idx_d = VIC::idx_d;
    /// Number of freely available bits
    static const int free_bits = VIC::free_bits;
    /// Number of used subscription entries
    unsigned int entries;
    /// Number of free subscription entries
    unsigned int free_and_bits;
    /// Maximal propagation condition
    static const Gecode::PropCond pc_max = VIC::pc_max;

    union {
      /**
       * \brief Indices of subscribed actors
       *
       * The entries from base[0] to base[idx[pc_max]] are propagators,
       * where the entries between base[idx[pc-1]] and base[idx[pc]] are
       * the propagators that have subscribed with propagation condition pc.
       *
       * The entries between base[idx[pc_max]] and base[idx[pc_max+1]] are the
       * advisors subscribed to the variable implementation.
       */
      unsigned int idx[pc_max+1];
      /// During cloning, points to the next copied variable
      VarImp<VIC>* next;
    } u;

    /// Return subscribed actor at index \a pc
    ActorLink** actor(PropCond pc);
    /// Return subscribed actor at index \a pc, where \a pc is non-zero
    ActorLink** actorNonZero(PropCond pc);
    /// Return reference to index \a pc, where \a pc is non-zero
    unsigned int& idx(PropCond pc);
    /// Return index \a pc, where \a pc is non-zero
    unsigned int idx(PropCond pc) const;

    /**
     * \brief Update copied variable \a x
     *
     * The argument \a sub gives the memory area where subscriptions are
     * to be stored.
     */
    void update(VarImp* x, ActorLink**& sub);
    /**
     * \brief Update all copied variables of this type
     *
     * The argument \a sub gives the memory area where subscriptions are
     * to be stored.
     */
    static void update(Space& home, ActorLink**& sub);

    /// Enter propagator to subscription array
    void enter(Space& home, Propagator* p, PropCond pc);
    /// Enter advisor to subscription array
    void enter(Space& home, Advisor* a);
    /// Resize subscription array
    void resize(Space& home);
    /// Remove propagator from subscription array
    void remove(Space& home, Propagator* p, PropCond pc);
    /// Remove advisor from subscription array
    void remove(Space& home, Advisor* a);

  protected:
#ifdef GECODE_HAS_VAR_DISPOSE
    /// Return reference to variables (dispose)
    static VarImp<VIC>* vars_d(Space& home);
    /// Set reference to variables (dispose)
    static void vars_d(Space& home, VarImp<VIC>* x);
#endif

  public:
    /// Creation
    VarImp(Space& home);
    /// Creation of static instances
    VarImp(void);

    /// \name Dependencies
    //@{
    /** \brief Subscribe propagator \a p with propagation condition \a pc
     *
     * In case \a schedule is false, the propagator is just subscribed but
     * not scheduled for execution (this must be used when creating
     * subscriptions during propagation).
     *
     * In case the variable is assigned (that is, \a assigned is 
     * true), the subscribing propagator is scheduled for execution.
     * Otherwise, the propagator subscribes and is scheduled for execution
     * with modification event \a me provided that \a pc is different
     * from \a PC_GEN_ASSIGNED.
     */
    void subscribe(Space& home, Propagator& p, PropCond pc,
                   bool assigned, ModEvent me, bool schedule);
    /** \brief Cancel subscription of propagator \a p with propagation condition \a pc
     *
     * If the variable is assigned, \a assigned must be true.
     *
     */
    void cancel(Space& home, Propagator& p, PropCond pc,
                bool assigned);
    /** \brief Subscribe advisor \a a to variable
     *
     * The advisor \a a is only subscribed if \a assigned is false.
     *
     */
    void subscribe(Space& home, Advisor& a, bool assigned);
    /** \brief Cancel subscription of advisor \a a
     *
     * If the variable is assigned, \a assigned must be true.
     *
     */
    void cancel(Space& home, Advisor& a, bool assigned);
    /// Cancel all subscriptions when variable implementation is assigned
    void cancel(Space& home);
    /**
     * \brief Return degree (number of subscribed propagators and advisors)
     *
     * Note that the degree of a variable implementation is not available
     * during copying.
     */
    unsigned int degree(void) const;
    /**
     * \brief Run advisors when variable implementation has been modified with modification event \a me and domain change \a d
     *
     * Returns false if an advisor has failed.
     */
    bool advise(Space& home, ModEvent me, Delta& d);
    //@}

    /// \name Cloning variables
    //@{
    /// Constructor for cloning
    VarImp(Space& home, bool share, VarImp& x);
    /// Is variable already copied
    bool copied(void) const;
    /// Use forward pointer if variable already copied
    VarImp* forward(void) const;
    /// Return next copied variable
    VarImp* next(void) const;
    //@}

    /// \name Variable implementation-dependent propagator support
    //@{
    /// Schedule propagator \a p with modification event \a me
    static void schedule(Space& home, Propagator& p, ModEvent me);
    /// Project modification event for this variable type from \a med
    static ModEvent me(const ModEventDelta& med);
    /// Translate modification event \a me into modification event delta
    static ModEventDelta med(ModEvent me);
    /// Combine modifications events \a me1 and \a me2
    static ModEvent me_combine(ModEvent me1, ModEvent me2);
    //@}

    /// Provide access to free bits
    unsigned int bits(void) const;
    /// Provide access to free bits
    unsigned int& bits(void);

  protected:
    /// Schedule subscribed propagators
    void schedule(Space& home, PropCond pc1, PropCond pc2, ModEvent me);

  public:
    /// \name Memory management
    //@{
    /// Allocate memory from space
    static void* operator new(size_t,Space&);
    /// Return memory to space
    static void  operator delete(void*,Space&);
    /// Needed for exceptions
    static void  operator delete(void*);
    //@}
    
    /// \name Reflection
    //@{
    /// Variable type identifier
    static const Support::Symbol vti;
    //@}
    
  };

  template <class VIC>
  const Support::Symbol
  VarImp<VIC>::vti = VIC::vti;


  namespace Reflection {
    class ActorSpecIter;
    class ActorSpec;
    class BranchingSpec;
    class VarMap;
  }

  /**
   * \defgroup TaskActorStatus Status of constraint propagation and branching commit
   * Note that the enum values starting with a double underscore should not
   * be used directly. Instead, use the provided functions with the same
   * name without leading underscores.
   *
   * \ingroup TaskActor
   */
  enum ExecStatus {
    __ES_SUBSUMED       = -2, ///< Internal: propagator is subsumed, do not use
    ES_FAILED           = -1, ///< Execution has resulted in failure
    ES_NOFIX            =  0, ///< Propagation has not computed fixpoint
    ES_OK               =  0, ///< Execution is okay
    ES_FIX              =  1, ///< Propagation has computed fixpoint
    __ES_PARTIAL        =  2  ///< Internal: propagator has computed partial fixpoint, do not use
  };

  /**
   * \brief Classification of propagation cost
   * \ingroup TaskActor
   */
  enum PropCost {
    PC_CRAZY_LO     = 0, ///< Exponential complexity, cheap
    PC_CRAZY_HI     = 0, ///< Exponential complexity, expensive
    PC_CUBIC_LO     = 1, ///< Cubic complexity, cheap
    PC_CUBIC_HI     = 1, ///< Cubic complexity, expensive
    PC_QUADRATIC_LO = 2, ///< Quadratic complexity, cheap
    PC_QUADRATIC_HI = 2, ///< Quadratic complexity, expensive
    PC_LINEAR_HI    = 3, ///< Linear complexity, expensive
    PC_LINEAR_LO    = 4, ///< Linear complexity, cheap
    PC_TERNARY_HI   = 5, ///< Three variables, expensive
    PC_BINARY_HI    = 6, ///< Two variables, expensive
    PC_TERNARY_LO   = 6, ///< Three variables, cheap
    PC_BINARY_LO    = 7, ///< Two variables, cheap
    PC_UNARY_LO     = 7, ///< Only single variable, cheap
    PC_UNARY_HI     = 7, ///< Only single variable, expensive
    PC_MAX          = 7  ///< Maximal cost value
  };

  /**
   * \brief Actor properties
   * \ingroup TaskActor
   */
  enum ActorProperty {
    /**
     * \brief Actor must always be disposed
     *
     * Normally, a propagator will not be disposed if its home space
     * is deleted. However, if an actor uses external resources,
     * this property can be used to make sure that the actor
     * will always be disposed.
     */ 
    AP_DISPOSE = (1 << 0),
    /**
     * Propagator is only weakly monotonic, that is, the propagator
     * is only monotonic on assignments.
     *
     */
    AP_WEAKLY  = (1 << 1)
  };


  /**
   * \brief Double-linked list for actors
   *
   * Used to maintain which actors belong to a space and also
   * (for propagators) to organize actors in the queue of
   * waiting propagators.
   */
  class ActorLink {
    friend class Actor;
    friend class Propagator;
    friend class Advisor;
    friend class Branching;
    friend class Space;
    template <class VIC> friend class VarImp;
  private:
    ActorLink* _next; ActorLink* _prev;
  public:
    //@{
    /// Routines for double-linked list
    ActorLink* prev(void) const; void prev(ActorLink*);
    ActorLink* next(void) const; void next(ActorLink*);
    ActorLink** next_ref(void);
    //@}

    /// Initialize links (self-linked)
    void init(void);
    /// Remove from predecessor and successor
    void unlink(void);
    /// Insert \a al directly after this
    void head(ActorLink* al);
    /// Insert \a al directly before this
    void tail(ActorLink* al);
    /// Static cast for a non-null pointer (to give a hint to optimizer)
    template <class T> static ActorLink* cast(T* a);
    /// Static cast for a non-null pointer (to give a hint to optimizer)
    template <class T> static const ActorLink* cast(const T* a);
  };


  /**
   * \brief Base-class for both propagators and branchings
   * \ingroup TaskActor
   */
  class GECODE_VTABLE_EXPORT Actor : private ActorLink {
    friend class ActorLink;
    friend class Space;
    friend class Propagator;
    friend class Advisor;
    friend class Branching;
    friend class Reflection::ActorSpecIter;
    template <class VIC> friend class VarImp;
    template <class A> friend class Council;
  private:
    /// Static cast for a non-null pointer (to give a hint to optimizer)
    static Actor* cast(ActorLink* al);
    /// Static cast for a non-null pointer (to give a hint to optimizer)
    static const Actor* cast(const ActorLink* al);
  public:
    /// Create copy
    virtual Actor* copy(Space& home, bool share) = 0;

    /// \name Memory management
    //@{
    /// Report size occupied by additionally datastructures
    GECODE_KERNEL_EXPORT 
    virtual size_t allocated(void) const;
    /// Delete actor and return its size
    GECODE_KERNEL_EXPORT
    virtual size_t dispose(Space& home);
    /// Allocate memory from space
    static void* operator new(size_t s, Space& home);
    /// No-op for exceptions
    static void  operator delete(void* p, Space& home);
    /// Return specification for this actor given a variable map \a m
    GECODE_KERNEL_EXPORT
    virtual Reflection::ActorSpec spec(const Space& home,
                                       Reflection::VarMap& m) const;
  private:
#ifndef __GNUC__
    /// Not used (uses dispose instead)
    static void  operator delete(void* p);
#endif
    /// Not used
    static void* operator new(size_t s);
    //@}
#ifdef __GNUC__
  public:
    /// To avoid warnings from GCC
    GECODE_KERNEL_EXPORT virtual ~Actor(void);
    /// Not used (uses dispose instead)
    static void  operator delete(void* p);
#endif
  };


  /**
   * \brief %Propagator \a p is subsumed
   *
   * The size of the propagator is \a s.
   *
   * Note that the propagator must be subsumed and also disposed. So
   * in general, there should be code such as 
   * \code return ES_SUBSUMED(*this,dispose(home)) \endcode.
   * 
   * However, in case the propagator has nothing to dispose (all its
   * views are assigned and no external resources) it is sufficient
   * to do 
   * \code return ES_SUBSUMED(*this,sizeof(*this)) \endcode.
   *
   * \warning Has a side-effect on the propagator. Overwrites
   *          the modification event delta of a propagator.
   *          Use only directly with returning from propagation.
   * \ingroup TaskActorStatus
   */
  ExecStatus ES_SUBSUMED(Propagator& p, size_t s);
  /**
   * \brief %Propagator \a p is subsumed
   *
   * First disposes the propagator and then returns subsumption.
   *
   * \warning Has a side-effect on the propagator. Overwrites
   *          the modification event delta of a propagator.
   *          Use only directly with returning from propagation.
   * \ingroup TaskActorStatus
   */
  ExecStatus ES_SUBSUMED(Propagator& p, Space& home);
  /**
   * \brief %Propagator \a p has computed partial fixpoint
   *
   * %Set modification event delta to \a med and schedule propagator
   * accordingly.
   *
   * \warning Has a side-effect on the propagator. 
   *          Use only directly with returning from propagation.
   * \ingroup TaskActorStatus
   */
  ExecStatus ES_FIX_PARTIAL(Propagator& p, const ModEventDelta& med);
  /**
   * \brief %Propagator \a p has not computed partial fixpoint
   *
   * Combine current modification event delta with \a and schedule
   * propagator accordingly.
   *
   * \warning Has a side-effect on the propagator.
   *          Use only directly with returning from propagation.
   * \ingroup TaskActorStatus
   */
  ExecStatus ES_NOFIX_PARTIAL(Propagator& p, const ModEventDelta& med);

  /**
   * \brief Base-class for propagators
   * \ingroup TaskActor
   */
  class GECODE_VTABLE_EXPORT Propagator : public Actor {
    friend class ActorLink;
    friend class Space;
    template <class VIC> friend class VarImp;
    friend ExecStatus ES_SUBSUMED(Propagator&, size_t);
    friend ExecStatus ES_SUBSUMED(Propagator&, Space&);
    friend ExecStatus ES_FIX_PARTIAL(Propagator&, const ModEventDelta&);
    friend ExecStatus ES_NOFIX_PARTIAL(Propagator&, const ModEventDelta&);
    friend class Advisor;
    template <class A> friend class Council;
  private:
    union {
      /// A set of modification events (used during propagation)
      ModEventDelta med;
      /// The size of the propagator (used during subsumption)
      size_t size;
      /// A list of advisors (used during cloning)
      Gecode::ActorLink* advisors;
    } u;
    /// Static cast for a non-null pointer (to give a hint to optimizer)
    static Propagator* cast(ActorLink* al);
    /// Static cast for a non-null pointer (to give a hint to optimizer)
    static const Propagator* cast(const ActorLink* al);
  protected:
    /// Constructor for creation
    Propagator(Space& home);
    /// Constructor for cloning \a p
    Propagator(Space& home, bool share, Propagator& p);

  public:
    /// \name Propagation
    //@{
    /**
     * \brief Propagation function
     *
     * The propagation function must return an execution status as
     * follows:
     *  - ES_FAILED: the propagator has detected failure
     *  - ES_NOFIX: the propagator has done propagation
     *  - ES_FIX: the propagator has done propagation and has computed
     *    a fixpoint. That is, running the propagator immediately
     *    again will do nothing.
     *
     * Apart from the above values, a propagator can return
     * the result from calling one of the functions 
     *  - ES_SUBSUMED: the propagator is subsumed and has been already
     *    deleted.
     *  - ES_NOFIX_PARTIAL: the propagator has consumed some of its
     *    propagation events.
     *  - ES_FIX_PARTIAL: the propagator has consumed some of its
     *    propagation events and with respect to these events is
     *    at fixpoint
     * For more details, see the individual functions.
     *
     */
    virtual ExecStatus propagate(Space& home, const ModEventDelta& med) = 0;
    /// Cost function
    virtual PropCost cost(const Space& home, const ModEventDelta& med) const = 0;
    /**
     * \brief Advise function
     *
     * The advisor is passed as argument \a a.
     *
     * A propagator must specialize this advise function, if it
     * uses advisors. The advise function must return an execution
     * status as follows:
     *  - ES_FAILED: the advisor has detected failure
     *  - ES_FIX: the advisor's propagator (that is, this propagator) 
     *    does not need to be run
     *  - ES_NOFIX: the advisor's propagator (that is, this propagator)
     *    must be run
     *
     * Apart from the above values, an advisor can return
     * the result from calling the function
     *  - ES_SUBSUMED_FIX: the advisor is subsumed, the advisor's
     *    propagator does not need to be run
     *  - ES_SUBSUMED_NOFIX: the advisor is subsumed, the advisor's
     *    propagator must be run
     * For more details, see the function documentation.
     *
     * The delta \a d describes how the variable has been changed
     * by an operation on the advisor's variable. Typically,
     * the delta information can only be utilized by either 
     * static or member functions of views as the actual delta 
     * information is both domain and view dependent.
     *
     */
    GECODE_KERNEL_EXPORT
    virtual ExecStatus advise(Space& home, Advisor& a, const Delta& d);
    //@}
  };


  /** 
   * \brief Council of advisors
   * 
   * If a propagator uses advisors, it must maintain its advisors
   * through a council.
   * \ingroup TaskActor
   */
  template <class A>
  class Council {
    friend class Advisor;
    friend class Advisors<A>;
  private:
    /// Starting point for a linked list of advisors
    mutable ActorLink* advisors;
  public:
    /// Default constructor
    Council(void);
    /// Construct advisor council
    Council(Space& home);
    /// Test whether council has advisor left
    bool empty(void) const;
    /// Update during cloning (copies all advisors)
    void update(Space& home, bool share, Council<A>& c);
    /// Dispose council
    void dispose(Space& home);
  };


  /**
   * \brief Class to iterate over advisors of a council
   * \ingroup TaskActor
   */
  template <class A>
  class Advisors {
  private:
    /// The current advisor
    ActorLink* a;
  public:
    /// Initialize
    Advisors(const Council<A>& c);
    /// Test whether there advisors left
    bool operator ()(void) const;
    /// Move iterator to next advisor
    void operator ++(void);
    /// Return advisor
    A& advisor(void) const;
  };


  /**
   * \brief %Advisor \a a is subsumed
   *
   * Disposes the advisor and: 
   *  - returns subsumption.
   *  - returns that the propagator of \a a need not be run.
   *
   * \warning Has a side-effect on the advisor. Use only directly when
   *          returning from advise.
   * \ingroup TaskActorStatus
   */
  template <class A>
  ExecStatus ES_SUBSUMED_FIX(A& a, Space& home, Council<A>& c);
  /**
   * \brief %Advisor \a a is subsumed
   *
   * Disposes the advisor and: 
   *  - returns subsumption.
   *  - returns that the propagator of \a a must be run.
   *
   * \warning Has a side-effect on the advisor. Use only directly when
   *          returning from advise.
   * \ingroup TaskActorStatus
   */
  template <class A>
  ExecStatus ES_SUBSUMED_NOFIX(A& a, Space& home, Council<A>& c);

  /**
   * \brief Base-class for advisors
   *
   * Advisors are typically subclassed for each propagator that
   * wants to use advisors. The actual member function that
   * is executed when a variable is changed, must be implemented
   * by the advisor's propagator.
   *
   * \ingroup TaskActor
   */
  class Advisor : private ActorLink {
    template <class VIC> friend class VarImp;
    template <class A> friend class Council;
    template <class A> friend class Advisors;
  private:
    /// Is the advisor disposed?
    bool disposed(void) const;
    /// Static cast
    static Advisor* cast(ActorLink* al);
    /// Static cast
    static const Advisor* cast(const ActorLink* al);
  protected:
    /// Return the advisor's propagator
    Propagator& propagator(void) const; 
  public:
    /// Constructor for creation
    template <class A>
    Advisor(Space& home, Propagator& p, Council<A>& c);
    /// Copying constructor
    Advisor(Space& home, bool share, Advisor& a);

    /// \name Memory management
    //@{
    /// Dispose the advisor
    template <class A>
    void dispose(Space& home, Council<A>& c);
    /// Allocate memory from space
    static void* operator new(size_t s, Space& home);
    /// No-op for exceptions
    static void  operator delete(void* p, Space& home);
    //@}
  private:
#ifndef __GNUC__
    /// Not used (uses dispose instead)
    static void  operator delete(void* p);
#endif
    /// Not used
    static void* operator new(size_t s);
  };


  class Branching;

  /**
   * \brief Branch description for batch recomputation
   *
   * Must be refined by inheritance such that the information stored
   * inside a branching description is sufficient to redo a tell
   * performed by a particular branching.
   *
   * \ingroup TaskActor
   */
  class BranchingDesc {
    friend class Space;
    friend class Reflection::BranchingSpec;
  private:
    unsigned int _id;  ///< Identity to match creating branching
    unsigned int _alt; ///< Number of alternatives

    /// Return id of the creating branching
    unsigned int id(void) const;
  protected:
    /// Initialize for particular branching \a b and alternatives \a a
    BranchingDesc(const Branching& b, const unsigned int a);
  public:
    /// Return number of alternatives
    unsigned int alternatives(void) const;
    /// Destructor
    GECODE_KERNEL_EXPORT virtual ~BranchingDesc(void);
    /// Report size occupied by branching description
    virtual size_t size(void) const = 0;
    /// Allocate memory from heap
    static void* operator new(size_t);
    /// Return memory to heap
    static void  operator delete(void*);
  };

  /**
   * \brief Base-class for branchings
   *
   * Note that branchings cannot be created inside a propagator
   * (no idea why one would like to do that anyway). If you do that
   * the system will explode in a truly interesting way.
   *
   * \ingroup TaskActor
   */
  class GECODE_VTABLE_EXPORT Branching : public Actor {
    friend class ActorLink;
    friend class Space;
    friend class BranchingDesc;
    friend class Reflection::ActorSpecIter;
  private:
    /// Unique identity (to match to branching descriptions)
    unsigned int id; 
    /// Static cast for a non-null pointer (to give a hint to optimizer)
    static Branching* cast(ActorLink* al);
    /// Static cast for a non-null pointer (to give a hint to optimizer)
    static const Branching* cast(const ActorLink* al);
  protected:
    /// Constructor for creation
    Branching(Space& home);
    /// Constructor for cloning \a b
    Branching(Space& home, bool share, Branching& b);

  public:
    /// \name Branching
    //@{
    /**
     * \brief Check status of branching, return true if alternatives left
     *
     * This method is called when Space::status is called, it determines
     * whether to continue branching with this branching or move on to
     * the (possibly) next branching.
     *
     */
    virtual bool status(const Space& home) const = 0;
    /**
     * \brief Return branching description
     *
     * Note that this method relies on the fact that it is called
     * immediately after a previous call to status. Moreover, the
     * member function can only be called once.
     */
    virtual const BranchingDesc* description(Space& home) = 0;
    /**
     * \brief Commit for branching description \a d and alternative \a a
     *
     * The current branching in the space \a home performs a commit from
     * the information provided by the branching description \a d
     * and the alternative \a a.
     */
    virtual ExecStatus commit(Space& home, const BranchingDesc& d,
                              unsigned int a) = 0;
    //@}
    
    /// \name Reflection
    //@{
    /// Specification for BranchingDesc \a d
    virtual GECODE_KERNEL_EXPORT Reflection::BranchingSpec
    branchingSpec(const Space& home,
                  Reflection::VarMap& m, const BranchingDesc& d) const;
    //@}
  };



  /** 
   * \brief %Space status
   * \ingroup TaskSearch
   */
  enum SpaceStatus {
    SS_FAILED, ///< %Space is failed
    SS_SOLVED, ///< %Space is solved (no branching left)
    SS_BRANCH  ///< %Space must be branched (at least one branching left)
  };

  /**
   * \brief Computation spaces
   */
  class GECODE_VTABLE_EXPORT Space {
    friend class Actor;
    friend class Propagator;
    friend class Branching;
    friend class Advisor;
    friend class Reflection::ActorSpecIter;
    template <class VIC> friend class VarImp;
    template <class VarType> friend class VarDisposer;
    friend class CopiedHandle;
    friend class Region;
  private:
    /// Performs memory management for space
    MemoryManager mm; 
    /// Shared region area
    SharedRegionArea* sra;
    /**
     * \brief Doubly linked list of all actors
     *
     * Propagators are stored at the beginning, branchings (if any) at
     * the end.
     */
    ActorLink a_actors;
    /**
     * \brief Points to the first branching to be used for status
     *
     * If equal to &a_actors, no branching does exist.
     */
    Branching* b_status;
    /**
     * \brief Points to the first branching to be used for commit
     *
     * Note that \a b_commit can point to an earlier branching
     * than \a b_status. This reflects the fact that the earlier
     * branching is already done (that is, status on that branching
     * returns false) but there might be still branching descriptions
     * referring to the earlier branching.
     *
     * If equal to &a_actors, no branching does exist.
     */
    Branching* b_commit;
    union {
      /// Data only available during propagation
      struct {
        /**
         * \brief Cost level with next propagator to be executed
         *
         * This maintains the following invariant (but only if the
         * space does not perform propagation):
         *  - If active points to a queue, this queue might contain
         *    a propagator. However, there will be at least one queue
         *    containing a propagator.
         *  - Otherwise, active is smaller than the beginning of
         *    the queues. Then, the space is stable.
         *  - If active is NULL, the space is failed.
         */
        ActorLink* active;
        /// Scheduled propagators according to cost
        ActorLink queue[PC_MAX+1];
        /// Id of next branching to be created
        unsigned int branch_id;
        /// Number of subscriptions
        unsigned int n_sub;
      } p;
      /// Data available only during copying
      struct {
        /// Entries for updating variables
        VarImpBase* vars_u[AllVarConf::idx_c];
        /// Keep variables during copying without index structure
        VarImpBase* vars_noidx;
        /// Linked list of copied objects
        CopiedHandle::Object* copied;
      } c;
    } pc;
    /// Put propagator \a p into right queue
    void enqueue(Propagator* p);
    /**
     * \name update, and dispose variables
     */
    //@{
#ifdef GECODE_HAS_VAR_DISPOSE
    /// Registered variable type disposers
    GECODE_KERNEL_EXPORT static VarDisposerBase* vd[AllVarConf::idx_d];
    /// Entries for disposing variables
    VarImpBase* _vars_d[AllVarConf::idx_d];
    /// Return reference to variables (dispose)
    template <class VIC> VarImpBase* vars_d(void) const;
    /// Set reference to variables (dispose)
    template <class VIC> void vars_d(VarImpBase* x);
#endif
    /// Update all cloned variables
    void update(ActorLink** sub);
    //@}

    /// First actor for forced disposal
    Actor** d_fst;
    /// Current actor for forced disposal
    Actor** d_cur;
    /// Last actor for forced disposal
    Actor** d_lst;
    /// Resize disposal array
    GECODE_KERNEL_EXPORT void d_resize(void);

    /**
     * \brief Number of weakly monotonic propagators
     *
     * If zero, none exists. If one, then none exists right now but
     * there has been one since the last fixpoint computed. Otherwise,
     * it gives the number of weakly monotoning propagators minus one.
     */
    unsigned int n_wmp;

    /// Used for default arguments
    GECODE_KERNEL_EXPORT static unsigned long int unused_uli;
    /// Used for default arguments
    GECODE_KERNEL_EXPORT static bool unused_b;

    /**
     * \brief Query space status
     *
     * Propagates the space until fixpoint or failure and
     * increments \a pn by the number of propagator executions and
     * returns:
     *  - if the space is failed, SpaceStatus::SS_FAILED is returned.
     *  - if the space is not failed but the space has no branching left,
     *    SpaceStatus::SS_SOLVED is returned.
     *  - otherwise, SpaceStatus::SS_BRANCH is returned.
     */
    GECODE_KERNEL_EXPORT SpaceStatus _status(unsigned long int& pn=unused_uli);
  public:
    /**
     * \brief Default constructor
     * \ingroup TaskModelScript
     */
    GECODE_KERNEL_EXPORT Space(void);
    /**
     * \brief Destructor
     * \ingroup TaskModelScript
     */
    GECODE_KERNEL_EXPORT virtual ~Space(void);
    /**
     * \brief Constructor for cloning
     *
     * Must copy and update all data structures (such as variables
     * and variable arrays) required by the subclass of Space.
     *
     * If \a share is true, share all data structures among copies.
     * Otherwise, make independent copies.
     * \ingroup TaskModelScript
     */
    GECODE_KERNEL_EXPORT Space(bool share, Space& s);
    /**
     * \brief Copying member function
     *
     * Must create a new object using the constructor for cloning.
     * \ingroup TaskModelScript
     */
    virtual Space* copy(bool share) = 0;
    /**
     * \brief Constrain function for best solution search
     *
     * Must constrain this space to be better than the so far best
     * solution \a best.
     *
     * If best solution search is used and this method is not redefined,
     * an exception of type SpaceConstrainUndefined is thrown.
     *
     * \ingroup TaskModelScript
     */
    GECODE_KERNEL_EXPORT virtual void constrain(const Space& best);
    /**
     * \brief Allocate memory from heap for new space
     * \ingroup TaskModelScript
     */
    static void* operator new(size_t);
    /**
     * \brief Free memory allocated from heap
     * \ingroup TaskModelScript
     */
    static void  operator delete(void*);


    /*
     * Member functions for search engines
     *
     */

    /**
     * \brief Query space status
     *
     * Propagates the space until fixpoint or failure;
     * increments \a pn by the number of propagator executions;
     * sets \a wmp to true, if the space has weakly monotonic propagators or
     * a weakly monotonic propagator might have been used in propagation; and:
     *  - if the space is failed, SpaceStatus::SS_FAILED is returned.
     *  - if the space is not failed but the space has no branching left,
     *    SpaceStatus::SS_SOLVED is returned.
     *  - otherwise, SpaceStatus::SS_BRANCH is returned.
     * \ingroup TaskSearch
     */
    SpaceStatus status(unsigned long int& pn=unused_uli, bool& wmp=unused_b);

    /**
     * \brief Create new branching description for current branching
     *
     * This member function can only be called after the member function
     * Space::status on the same space has been called and in between
     * no non-const member function has been called on this space.
     *
     * Moreover, the member function can only be called at most once
     * (otherwise, it might generate conflicting descriptions).
     *
     * Note that the above invariant only pertains to calls of member
     * functions of the same space. If the invariant is violated, the
     * system is likely to crash (hopefully it does). In particular, if
     * applied to a space with no current branching, the system will
     * crash.
     *
     * Throws an exception of type SpaceNotStable when applied to a not
     * yet stable space.
     *
     * \ingroup TaskSearch
     */
    const BranchingDesc* description(void);

    /**
     * \brief Clone space
     *
     * Assumes that the space is stable and not failed. If the space is
     * failed, an exception of type SpaceFailed is thrown. If the space
     * is not stable, an exception of SpaceNotStable is thrown.
     *
     * Otherwise, a clone of the space is returned. If \a share is true,
     * sharable datastructures are shared among the clone and the original
     * space. If \a share is false, independent copies of the shared
     * datastructures must be created. This means that a clone with no
     * sharing can be used in a different thread without any interaction
     * with the original space.
     *
     * \ingroup TaskSearch
     */
    GECODE_KERNEL_EXPORT Space* clone(bool share=true);

    /**
     * \brief Commit branching description \a d and for alternative \a a
     *
     * The current branching in the space performs a commit from
     * the information provided by the branching description \a d
     * and the alternative \a a.
     *
     * Note that no propagation is perfomed (to support batch
     * recomputation), in order to perform propagation the member
     * function status must be used.
     *
     * Committing with branching descriptions must be carried
     * out in the same order as the branch descriptions have been
     * obtained by the member function Space::description().
     *
     * It is perfectly okay to add constraints interleaved with
     * branching descriptions (provided they are in the right order).
     * However, if propagation is performed by calling the member
     * function status and then new branching descriptions are
     * computed, these branching descriptions are different.
     *
     * Committing throws the following exceptions:
     *  - SpaceNoBranching, if the space has no current branching (it is
     *    already solved).
     *  - SpaceIllegalAlternative, if \a a is not smaller than the number
     *    of alternatives supported by the branching description \a d.
     *
     * \ingroup TaskSearch
     */
    GECODE_KERNEL_EXPORT void commit(const BranchingDesc& d, unsigned int a);

    /**
     * \brief Notice actor property
     *
     * Make the space notice that the actor \a a has the property \a p.
     * Note that the same property can only be noticed once for an actor.
     * \ingroup TaskActor
     */
    void notice(Actor& a, ActorProperty p);
    /**
     * \brief Ignore actor property
     *
     * Make the space ignore that the actor \a a has the property \a p.
     * Note that a property must be ignored before an actor is disposed.
     * \ingroup TaskActor
     */
    void ignore(Actor& a, ActorProperty p);

    /**
     * \brief Fail space
     *
     * This is useful for failing outside of actors. Never use inside
     * a propagate or commit member function. The system will crash!
     * \ingroup TaskActor
     */
    void fail(void);
    /**
     * \brief Check whether space is failed
     *
     * Note that this does not perform propagation. This is useful
     * for posting actors: only if a space is not yet failed, new
     * actors are allowed to be created.
     * \ingroup TaskActor
     */
    bool failed(void) const;
    /**
     * \brief Return if space is stable (at fixpoint or failed)
     * \ingroup TaskActor
     */
    bool stable(void) const;
    /**
     * \brief Return number of propagators
     *
     * Note that this function takes linear time in the number of
     * propagators.
     */
    GECODE_KERNEL_EXPORT unsigned int propagators(void) const;
    /**
     * \brief Return number of branchings
     *
     * Note that this function takes linear time in the number of
     * branchings.
     */
    GECODE_KERNEL_EXPORT unsigned int branchings(void) const;

    /**
     * \name Reflection
     */
    //@{
    /// Enter variables into \a m
    GECODE_KERNEL_EXPORT
    virtual void getVars(Reflection::VarMap& m, bool registerOnly);
    /// Get reflection for BranchingDesc \a d
    GECODE_KERNEL_EXPORT
    Reflection::BranchingSpec branchingSpec(Reflection::VarMap& m,
                                            const BranchingDesc& d) const;
    //@}

    /**
     * \defgroup FuncMemSpace Space-memory management
     * \ingroup FuncMem
     */
    //@{
    /** 
     * \brief Allocate block of \a n objects of type \a T from space heap
     *
     * Note that this function implements C++ semantics: the default
     * constructor of \a T is run for all \a n objects.
     */
    template <class T>
    T* alloc(unsigned int n);
    /** 
     * \brief Delete \a n objects allocated from space heap starting at \a b
     *
     * Note that this function implements C++ semantics: the destructor
     * of \a T is run for all \a n objects.
     *
     * Note that the memory is not freed, it is just scheduled for latter
     * reusal.
     */
    template <class T>
    void free(T* b, unsigned int n);
    /**
     * \brief Reallocate block of \a n objects starting at \a b to \a m objects of type \a T from the space heap
     *
     * Note that this function implements C++ semantics: the copy constructor
     * of \a T is run for all \f$\min(n,m)\f$ objects, the default
     * constructor of \a T is run for all remaining 
     * \f$\max(n,m)-\min(n,m)\f$ objects, and the destrucor of \a T is
     * run for all \a n objects in \a b.
     *
     * Returns the address of the new block.
     */
    template <class T>
    T* realloc(T* b, unsigned int n, unsigned int m);
    /**
     * \brief Reallocate block of \a n pointers starting at \a b to \a m objects of type \a T* from the space heap
     *
     * Returns the address of the new block.
     *
     * This is a specialization for performance.
     */
    template <class T>
    T** realloc(T** b, unsigned int, unsigned int m);
    /// Allocate memory on space heap
    void* ralloc(size_t s);
    /// Free memory previously allocated with alloc (might be reused later)
    void rfree(void* p, size_t s);
    /// Reallocate memory block starting at \a b from size \a n to size \a s
    void* rrealloc(void* b, size_t n, size_t m);
    /// Allocate from freelist-managed memory
    template <size_t> void* fl_alloc(void);
    /**
     * \brief Return freelist-managed memory to freelist
     *
     * The first list element to be retuned is \a f, the last is \a l.
     */
    template <size_t> void  fl_dispose(FreeList* f, FreeList* l);
    /**
     * \brief Return how much heap memory is allocated
     *
     * Note that is includes both the memory allocated for the space heap
     * as well as additional memory allocated by actors.
     */
    GECODE_KERNEL_EXPORT 
    size_t allocated(void) const;
    //@}
  };




  /*
   * Memory management
   *
   */

  // Heap allocated
  forceinline void* 
  SharedHandle::Object::operator new(size_t s) {
    return heap.ralloc(s);
  }
  forceinline void
  SharedHandle::Object::operator delete(void* p) {
    heap.rfree(p);
  }

  forceinline void*
  Space::operator new(size_t s) {
    return heap.ralloc(s);
  }
  forceinline void
  Space::operator delete(void* p) {
    heap.rfree(p);
  }

  forceinline void
  BranchingDesc::operator delete(void* p) {
    heap.rfree(p);
  }
  forceinline void*
  BranchingDesc::operator new(size_t s) {
    return heap.ralloc(s);
  }

  // Space allocation: general space heaps and free lists
  forceinline void*
  Space::ralloc(size_t s) {
    return mm.alloc(s);
  }
  forceinline void
  Space::rfree(void* p, size_t s) {
    return mm.reuse(p,s);
  }
  forceinline void*
  Space::rrealloc(void* _b, size_t n, size_t m) {
    char* b = static_cast<char*>(_b);
    if (n < m) {
      char* p = static_cast<char*>(ralloc(m));
      memcpy(p,b,n);
      rfree(b,n);
      return p;
    } else {
      rfree(b+m,m-n);
      return b;
    }
  }

  template <size_t s>
  forceinline void*
  Space::fl_alloc(void) {
    return mm.template fl_alloc<s>();
  }
  template <size_t s>
  forceinline void
  Space::fl_dispose(FreeList* f, FreeList* l) {
    mm.template fl_dispose<s>(f,l);
  }

  /*
   * Typed allocation routines
   *
   */
  template <class T>
  forceinline T*
  Space::alloc(unsigned int n) {
    T* p = static_cast<T*>(ralloc(sizeof(T)*n));
    for (unsigned int i=n; i--; )
      (void) new (p+i) T();
    return p;
  }

  template <class T>
  forceinline void
  Space::free(T* b, unsigned int n) {
    for (unsigned int i=n; i--; )
      b[i].~T();
    rfree(b,n*sizeof(T));
  }

  template <class T>
  forceinline T*
  Space::realloc(T* b, unsigned int n, unsigned int m) {
    if (n < m) {
      T* p = static_cast<T*>(ralloc(sizeof(T)*m));
      for (unsigned int i=n; i--; )
        (void) new (p+i) T(b[i]);
      for (unsigned int i=n; i<m; i++)
        (void) new (p+i) T();
      free<T>(b,n);
      return p;
    } else {
      free<T>(b+m,m-n);
      return b;
    }
  }

#define GECODE_KERNEL_REALLOC(T)                                        \
  template <>                                                           \
  forceinline T*                                                        \
  Space::realloc<T>(T* b, unsigned int n, unsigned int m) {             \
    return static_cast<T*>(rrealloc(b,n*sizeof(T),m*sizeof(T)));        \
  }

  GECODE_KERNEL_REALLOC(bool)
  GECODE_KERNEL_REALLOC(signed char)
  GECODE_KERNEL_REALLOC(unsigned char)
  GECODE_KERNEL_REALLOC(signed short int)
  GECODE_KERNEL_REALLOC(unsigned short int)
  GECODE_KERNEL_REALLOC(signed int)
  GECODE_KERNEL_REALLOC(unsigned int)
  GECODE_KERNEL_REALLOC(signed long int)
  GECODE_KERNEL_REALLOC(unsigned long int)
  GECODE_KERNEL_REALLOC(float)
  GECODE_KERNEL_REALLOC(double)

#undef GECODE_KERNEL_REALLOC

  template <class T>
  forceinline T**
  Space::realloc(T** b, unsigned int n, unsigned int m) {
    return static_cast<T**>(rrealloc(b,n*sizeof(T),m*sizeof(T*)));
  }


#ifdef GECODE_HAS_VAR_DISPOSE
  template <class VIC>
  forceinline VarImpBase*
  Space::vars_d(void) const {
    return _vars_d[VIC::idx_d];
  }
  template <class VIC>
  forceinline void
  Space::vars_d(VarImpBase* x) {
    _vars_d[VIC::idx_d] = x;
  }
#endif

  // Space allocated entities: Actors, variable implementations, and advisors
  forceinline void
  Actor::operator delete(void*) {}
  forceinline void
  Actor::operator delete(void*, Space&) {}
  forceinline void*
  Actor::operator new(size_t s, Space& home) {
    return home.ralloc(s);
  }

  template <class VIC>
  forceinline void
  VarImp<VIC>::operator delete(void*) {}
  template <class VIC>
  forceinline void
  VarImp<VIC>::operator delete(void*, Space&) {}
  template <class VIC>
  forceinline void*
  VarImp<VIC>::operator new(size_t s, Space& home) {
    return home.ralloc(s);
  }

#ifndef __GNUC__
  forceinline void
  Advisor::operator delete(void*) {}
#endif
  forceinline void
  Advisor::operator delete(void*, Space&) {}
  forceinline void*
  Advisor::operator new(size_t s, Space& home) {
    return home.ralloc(s);
  }

  forceinline void
  CopiedHandle::Object::operator delete(void*, size_t) {}
  forceinline void
  CopiedHandle::Object::operator delete(void*, Space&) {}
  forceinline void*
  CopiedHandle::Object::operator new(size_t s, Space& home) {
    return home.ralloc(s);
  }

  /*
   * Copied objects and handles
   *
   */
  forceinline
  CopiedHandle::Object::Object(void) 
    : fwd(NULL) {}
  forceinline
  CopiedHandle::Object::~Object(void) {}

  forceinline 
  CopiedHandle::CopiedHandle(void) : o(NULL) {}
  forceinline 
  CopiedHandle::CopiedHandle(CopiedHandle::Object* so) : o(so) {}
  forceinline 
  CopiedHandle::CopiedHandle(const CopiedHandle& sh) : o(sh.o) {}
  forceinline CopiedHandle& 
  CopiedHandle::operator =(const CopiedHandle& sh) {
    o = sh.o;
    return *this;
  }
  forceinline void 
  CopiedHandle::update(Space& home, bool, CopiedHandle& sh) {
    if (sh.o == NULL) {
      o = NULL;
    } else if (sh.o->fwd != NULL) {
      o = sh.o->fwd;
    } else {
      o = sh.o->copy(); 
      sh.o->fwd = o;
      sh.o->next = home.pc.c.copied; 
      home.pc.c.copied = sh.o;
    }
  }
  forceinline void
  CopiedHandle::dispose(Space&) {
    (*o).~Object();
  }
  forceinline CopiedHandle::Object* 
  CopiedHandle::object(void) const {
    return o;
  }
  forceinline void 
  CopiedHandle::object(CopiedHandle::Object* n) {
    o=n;
  }

  /*
   * Shared objects and handles
   *
   */
  forceinline
  SharedHandle::Object::Object(void) 
    : use_cnt(0) {}
  forceinline
  SharedHandle::Object::~Object(void) {
    assert(use_cnt == 0);
  }

  forceinline SharedHandle::Object* 
  SharedHandle::object(void) const {
    return static_cast<SharedHandle::Object*>(CopiedHandle::object());
  }
  forceinline void 
  SharedHandle::subscribe(void) {
    if (object() != NULL) object()->use_cnt++;
  }
  forceinline void 
  SharedHandle::cancel(void) {
    if ((object() != NULL) && (--object()->use_cnt == 0))
      delete object();
    CopiedHandle::object(NULL);
  }
  forceinline void 
  SharedHandle::object(SharedHandle::Object* n) {
    if (n != object()) {
      cancel(); CopiedHandle::object(n); subscribe();
    }
  }
  forceinline 
  SharedHandle::SharedHandle(void) {}
  forceinline 
  SharedHandle::SharedHandle(SharedHandle::Object* so) : CopiedHandle(so) {
    subscribe();
  }
  forceinline 
  SharedHandle::SharedHandle(const SharedHandle& sh) : CopiedHandle(sh) {
    subscribe();
  }
  forceinline SharedHandle& 
  SharedHandle::operator =(const SharedHandle& sh) {
    if (&sh != this) {
      cancel(); CopiedHandle::object(sh.object()); subscribe();
    }
    return *this;
  }
  forceinline void 
  SharedHandle::update(Space& home, bool share, SharedHandle& sh) {
    if (sh.object() == NULL) {
      CopiedHandle::object(NULL);
    } else if (share) {
      CopiedHandle::object(sh.object()); subscribe();
    } else {
      CopiedHandle::update(home, share, sh);
      subscribe();
    }
  }
  forceinline 
  SharedHandle::~SharedHandle(void) {
    cancel();
  }



  /*
   * ActorLink
   *
   */
  forceinline ActorLink*
  ActorLink::prev(void) const {
    return _prev; 
  }

  forceinline ActorLink*
  ActorLink::next(void) const { 
    return _next; 
  }

  forceinline ActorLink**
  ActorLink::next_ref(void) { 
    return &_next; 
  }

  forceinline void
  ActorLink::prev(ActorLink* al) { 
    _prev = al; 
  }

  forceinline void
  ActorLink::next(ActorLink* al) { 
    _next = al; 
  }

  forceinline void
  ActorLink::unlink(void) {
    ActorLink* p = _prev; ActorLink* n = _next;
    p->_next = n; n->_prev = p;
  }

  forceinline void
  ActorLink::init(void) {
    _next = this; _prev =this;
  }

  forceinline void
  ActorLink::head(ActorLink* a) {
    // Inserts al at head of link-chain (that is, after this)
    ActorLink* n = _next;
    this->_next = a; a->_prev = this;
    a->_next = n; n->_prev = a;
  }

  forceinline void
  ActorLink::tail(ActorLink* a) {
    // Inserts al at tail of link-chain (that is, before this)
    ActorLink* p = _prev;
    a->_next = this; this->_prev = a;
    p->_next = a; a->_prev = p;
  }

  template <class T>
  forceinline ActorLink* 
  ActorLink::cast(T* a) {
    // Turning al into a reference is for gcc, assume is for MSVC
    GECODE_NOT_NULL(a);
    ActorLink& t = *a;
    return static_cast<ActorLink*>(&t);
  }

  template <class T>
  forceinline const ActorLink* 
  ActorLink::cast(const T* a) {
    // Turning al into a reference is for gcc, assume is for MSVC
    GECODE_NOT_NULL(a);
    const ActorLink& t = *a;
    return static_cast<const ActorLink*>(&t);
  }


  /*
   * Actor
   *
   */
  forceinline Actor* 
  Actor::cast(ActorLink* al) {
    // Turning al into a reference is for gcc, assume is for MSVC
    GECODE_NOT_NULL(al);
    ActorLink& t = *al;
    return static_cast<Actor*>(&t);
  }

  forceinline const Actor* 
  Actor::cast(const ActorLink* al) {
    // Turning al into a reference is for gcc, assume is for MSVC
    GECODE_NOT_NULL(al);
    const ActorLink& t = *al;
    return static_cast<const Actor*>(&t);
  }

  forceinline void
  Space::notice(Actor& a, ActorProperty p) {
    if (p & AP_DISPOSE) {
      if (d_cur == d_lst)
        d_resize();
      *(d_cur++) = &a;
    }
    if (p & AP_WEAKLY) {
      if (n_wmp == 0)
        n_wmp = 2;
      else
        n_wmp++;
    }
  }

  forceinline void
  Space::ignore(Actor& a, ActorProperty p) {
    if (p & AP_DISPOSE) {
      // Check wether array has already been discarded as space
      // deletion is already in progress
      Actor** f = d_fst;
      if (f != NULL) {
        while (&a != *f)
          f++;
        *f = *(--d_cur);
      }
    }
    if (p & AP_WEAKLY) {
      if (n_wmp == 2)
        n_wmp = 0;
      else
        n_wmp--;
    }
  }
  
  forceinline size_t
  Actor::dispose(Space&) {
    return sizeof(*this);
  }


  /*
   * Propagator
   *
   */
  forceinline Propagator* 
  Propagator::cast(ActorLink* al) {
    // Turning al into a reference is for gcc, assume is for MSVC
    GECODE_NOT_NULL(al);
    ActorLink& t = *al;
    return static_cast<Propagator*>(&t);
  }

  forceinline const Propagator* 
  Propagator::cast(const ActorLink* al) {
    // Turning al into a reference is for gcc, assume is for MSVC
    GECODE_NOT_NULL(al);
    const ActorLink& t = *al;
    return static_cast<const Propagator*>(&t);
  }

  forceinline
  Propagator::Propagator(Space& home) {
    u.advisors = NULL;
    assert(u.med == 0 && u.size == 0);
    home.a_actors.head(this);
  }

  forceinline
  Propagator::Propagator(Space&, bool, Propagator& p) {
    u.advisors = NULL;
    assert(u.med == 0 && u.size == 0);
    // Set forwarding pointer
    p.prev(this);
  }

  forceinline ExecStatus
  ES_SUBSUMED(Propagator& p, size_t s) {
    p.u.size = s; 
    return __ES_SUBSUMED;
  }

  forceinline ExecStatus
  ES_SUBSUMED(Propagator& p, Space& home) {
    p.u.size = p.dispose(home); 
    return __ES_SUBSUMED;
  }

  forceinline ExecStatus
  ES_FIX_PARTIAL(Propagator& p, const ModEventDelta& med) {
    p.u.med = med; 
    assert(p.u.med != 0);
    return __ES_PARTIAL;
  }

  forceinline ExecStatus
  ES_NOFIX_PARTIAL(Propagator& p, const ModEventDelta& med) {
    p.u.med = AllVarConf::med_combine(p.u.med,med); 
    assert(p.u.med != 0);
    return __ES_PARTIAL;
  }



  /*
   * Branching
   *
   */
  forceinline Branching* 
  Branching::cast(ActorLink* al) {
    // Turning al into a reference is for gcc, assume is for MSVC
    GECODE_NOT_NULL(al);
    ActorLink& t = *al;
    return static_cast<Branching*>(&t);
  }

  forceinline const Branching* 
  Branching::cast(const ActorLink* al) {
    // Turning al into a reference is for gcc, assume is for MSVC
    GECODE_NOT_NULL(al);
    const ActorLink& t = *al;
    return static_cast<const Branching*>(&t);
  }

  forceinline
  Branching::Branching(Space& home) {
    // Propagators are put at the tail of the link of actors
    id = home.pc.p.branch_id++;
    // If no branching available, make it the first one
    if (home.b_status == &(home.a_actors)) {
      home.b_status = this;
      if (home.b_commit == &(home.a_actors))
        home.b_commit = this;
    }
    home.a_actors.tail(this);
  }

  forceinline
  Branching::Branching(Space&, bool, Branching& b)
    : id(b.id)  {
    // Set forwarding pointer
    b.prev(this);
  }



  /*
   * Branching description
   *
   */
  forceinline
  BranchingDesc::BranchingDesc(const Branching& b, const unsigned int a)
    : _id(b.id), _alt(a) {}

  forceinline unsigned int
  BranchingDesc::alternatives(void) const {
    return _alt;
  }

  forceinline unsigned int
  BranchingDesc::id(void) const {
    return _id;
  }

  forceinline
  BranchingDesc::~BranchingDesc(void) {}



  /*
   * Delta information for advisors
   *
   */
  forceinline ModEvent
  Delta::modevent(void) const {
    return me;
  }



  /*
   * Advisor
   *
   */
  template <class A>
  forceinline 
  Advisor::Advisor(Space&, Propagator& p, Council<A>& c) {
    // Store propagator and forwarding in prev()
    ActorLink::prev(&p);
    // Link to next advisor in next()
    ActorLink::next(c.advisors); c.advisors = static_cast<A*>(this);
  }

  forceinline 
  Advisor::Advisor(Space&, bool, Advisor&) {}

  forceinline bool
  Advisor::disposed(void) const { 
    return prev() == NULL;
  }

  forceinline Advisor* 
  Advisor::cast(ActorLink* al) {
    return static_cast<Advisor*>(al);
  }

  forceinline const Advisor* 
  Advisor::cast(const ActorLink* al) {
    return static_cast<const Advisor*>(al);
  }

  forceinline Propagator&
  Advisor::propagator(void) const { 
    assert(!disposed());
    return *Propagator::cast(ActorLink::prev()); 
  }

  template <class A>
  forceinline void
  Advisor::dispose(Space&,Council<A>&) {
    assert(!disposed());
    ActorLink::prev(NULL); 
    // Shorten chains of disposed advisors by one, if possible
    Advisor* n = Advisor::cast(next());
    if ((n != NULL) && n->disposed())
      next(n->next());
  }

  template <class A>
  forceinline ExecStatus
  ES_SUBSUMED_FIX(A& a, Space& home, Council<A>& c) {
    a.dispose(home,c);
    return ES_FIX;
  }

  template <class A>
  forceinline ExecStatus
  ES_SUBSUMED_NOFIX(A& a, Space& home, Council<A>& c) {
    a.dispose(home,c);
    return ES_NOFIX;
  }



  /*
   * Advisor council
   *
   */
  template <class A>
  forceinline
  Council<A>::Council(void) {}

  template <class A>
  forceinline
  Council<A>::Council(Space&) 
    : advisors(NULL) {}

  template <class A>
  forceinline bool
  Council<A>::empty(void) const {
    ActorLink* a = advisors;
    while ((a != NULL) && static_cast<A*>(a)->disposed())
      a = a->next();
    advisors = a;
    return a == NULL;
  }

  template <class A>
  forceinline void
  Council<A>::update(Space& home, bool share, Council<A>& c) {
    // Skip all disposed advisors
    {
      ActorLink* a = c.advisors;
      while ((a != NULL) && static_cast<A*>(a)->disposed())
        a = a->next();
      c.advisors = a;
    }
    // Are there any advisors to be cloned?
    if (c.advisors != NULL) {
      // The propagator in from-space
      Propagator* p_f = &static_cast<A*>(c.advisors)->propagator();
      // The propagator in to-space
      Propagator* p_t = Propagator::cast(p_f->prev());
      // Advisors in from-space
      ActorLink** a_f = &c.advisors;
      // Advisors in to-space
      A* a_t = NULL;
      while (*a_f != NULL) {
        if (static_cast<A*>(*a_f)->disposed()) {
          *a_f = (*a_f)->next();
        } else {
          // Run specific copying part
          A* a = new (home) A(home,share,*static_cast<A*>(*a_f));
          // Set propagator pointer
          a->prev(p_t);
          // Set forwarding pointer
          (*a_f)->prev(a);
          // Link
          a->next(a_t);
          a_t = a;
          a_f = (*a_f)->next_ref();
        }
      }
      advisors = a_t;
      // Enter advisor link for reset
      assert(p_f->u.advisors == NULL);
      p_f->u.advisors = c.advisors;
    }
  }
  
  template <class A>
  forceinline void
  Council<A>::dispose(Space& home) {
    ActorLink* a = advisors;
    while (a != NULL) {
      if (!static_cast<A*>(a)->disposed())
        static_cast<A*>(a)->dispose(home,*this); 
      a = a->next();
    }
  }

  

  /*
   * Advisor iterator
   *
   */
  template <class A>
  forceinline
  Advisors<A>::Advisors(const Council<A>& c)
    : a(c.advisors) {
    while ((a != NULL) && static_cast<A*>(a)->disposed())
      a = a->next();
  }

  template <class A>
  forceinline bool
  Advisors<A>::operator ()(void) const {
    return a != NULL;
  }

  template <class A>
  forceinline void
  Advisors<A>::operator ++(void) {
    do {
      a = a->next();
    } while ((a != NULL) && static_cast<A*>(a)->disposed());
  }

  template <class A>
  forceinline A&
  Advisors<A>::advisor(void) const {
    return *static_cast<A*>(a);
  }



  /*
   * Space
   *
   */
  forceinline void
  Space::enqueue(Propagator* p) {
    ActorLink::cast(p)->unlink();
    ActorLink* c = &pc.p.queue[p->cost(*this,p->u.med)];
    c->tail(ActorLink::cast(p));
    if (c > pc.p.active)
      pc.p.active = c;
  }

  forceinline void
  Space::fail(void) {
    pc.p.active = NULL;
  }

  forceinline bool
  Space::failed(void) const {
    return pc.p.active == NULL;
  }

  forceinline bool
  Space::stable(void) const {
    return pc.p.active < &pc.p.queue[0];
  }

  forceinline const BranchingDesc*
  Space::description(void) {
    if (!stable())
      throw SpaceNotStable("Space::description");
    return b_status->description(*this);
  }



  /*
   * Variable implementation
   *
   */
  template <class VIC>
  forceinline ActorLink**
  VarImp<VIC>::actor(PropCond pc) {
    assert((pc >= 0)  && (pc < pc_max+2));
    return (pc == 0) ? base : base+u.idx[pc-1];
  }

  template <class VIC>
  forceinline ActorLink**
  VarImp<VIC>::actorNonZero(PropCond pc) {
    assert((pc > 0)  && (pc < pc_max+2));
    return base+u.idx[pc-1];
  }

  template <class VIC>
  forceinline unsigned int&
  VarImp<VIC>::idx(PropCond pc) {
    assert((pc > 0)  && (pc < pc_max+2));
    return u.idx[pc-1];
  }

  template <class VIC>
  forceinline unsigned int
  VarImp<VIC>::idx(PropCond pc) const {
    assert((pc > 0)  && (pc < pc_max+2));
    return u.idx[pc-1];
  }

  template <class VIC>
  forceinline
  VarImp<VIC>::VarImp(Space&) {
    base = NULL; entries = 0;
    for (PropCond pc=1; pc<pc_max+2; pc++)
      idx(pc) = 0;
    free_and_bits = 0;
  }

  template <class VIC>
  forceinline
  VarImp<VIC>::VarImp(void) {
    base = NULL; entries = 0;
    for (PropCond pc=1; pc<pc_max+2; pc++)
      idx(pc) = 0;
    free_and_bits = 0;
  }

  template <class VIC>
  forceinline unsigned int
  VarImp<VIC>::degree(void) const {
    assert(!copied());
    return entries;
  }

  template <class VIC>
  forceinline unsigned int
  VarImp<VIC>::bits(void) const {
    return free_and_bits;
  }

  template <class VIC>
  forceinline unsigned int&
  VarImp<VIC>::bits(void) {
    return free_and_bits;
  }

#ifdef GECODE_HAS_VAR_DISPOSE
  template <class VIC>
  forceinline VarImp<VIC>*
  VarImp<VIC>::vars_d(Space& home) {
    return static_cast<VarImp<VIC>*>(home.vars_d<VIC>());
  }

  template <class VIC>
  forceinline void
  VarImp<VIC>::vars_d(Space& home, VarImp<VIC>* x) {
    home.vars_d<VIC>(x);
  }
#endif

  template <class VIC>
  forceinline bool
  VarImp<VIC>::copied(void) const {
    return Support::marked(base);
  }

  template <class VIC>
  forceinline VarImp<VIC>*
  VarImp<VIC>::forward(void) const {
    assert(copied());
    return reinterpret_cast<VarImp<VIC>*>(Support::unmark(base));
  }

  template <class VIC>
  forceinline VarImp<VIC>*
  VarImp<VIC>::next(void) const {
    assert(copied());
    return u.next;
  }

  template <class VIC>
  forceinline
  VarImp<VIC>::VarImp(Space& home, bool, VarImp<VIC>& x) {
    VarImpBase** reg;
    free_and_bits = x.free_and_bits & ((1 << free_bits) - 1);
    if (x.base == NULL) {
      // Variable implementation needs no index structure
      reg = &home.pc.c.vars_noidx;
      assert(x.degree() == 0);
    } else {
      reg = &home.pc.c.vars_u[idx_c];
    }
    // Save subscriptions in copy
    base = x.base;
    entries = x.entries;
    for (PropCond pc=1; pc<pc_max+2; pc++)
      idx(pc) = x.idx(pc);

    // Set forwarding pointer
    x.base = reinterpret_cast<ActorLink**>(Support::mark(this));
    // Register original
    x.u.next = static_cast<VarImp<VIC>*>(*reg); *reg = &x;
  }

  template <class VIC>
  forceinline ModEvent
  VarImp<VIC>::me(const ModEventDelta& med) {
    return static_cast<ModEvent>((med & VIC::med_mask) >> VIC::med_fst);
  }

  template <class VIC>
  forceinline ModEventDelta
  VarImp<VIC>::med(ModEvent me) {
    return static_cast<ModEventDelta>(me << VIC::med_fst);
  }

  template <class VIC>
  forceinline ModEvent
  VarImp<VIC>::me_combine(ModEvent me1, ModEvent me2) {
    return VIC::me_combine(me1,me2);
  }

  template <class VIC>
  forceinline void
  VarImp<VIC>::schedule(Space& home, Propagator& p, ModEvent me) {
    if (VIC::med_update(p.u.med,me))
      home.enqueue(&p);
  }

  template <class VIC>
  forceinline void
  VarImp<VIC>::schedule(Space& home, PropCond pc1, PropCond pc2, ModEvent me) {
    ActorLink** b = actor(pc1);
    ActorLink** p = actorNonZero(pc2+1);
    while (p-- > b)
      schedule(home,*Propagator::cast(*p),me);
  }

  template <class VIC>
  forceinline void
  VarImp<VIC>::enter(Space& home, Propagator* p, PropCond pc) {
    assert(pc <= pc_max);
    // Count one new subscription
    home.pc.p.n_sub += 1;
    if ((free_and_bits >> free_bits) == 0)
      resize(home);
    free_and_bits -= 1 << free_bits;

    // Enter subscription
    base[entries] = *actorNonZero(pc_max+1);
    entries++;
    for (PropCond j = pc_max; j > pc; j--) {
      *actorNonZero(j+1) = *actorNonZero(j);
      idx(j+1)++;
    }
    *actorNonZero(pc+1) = *actor(pc);
    idx(pc+1)++;
    *actor(pc) = ActorLink::cast(p);

#ifdef GECODE_AUDIT    
    ActorLink** f = actor(pc);
    while (f < (pc == pc_max+1 ? base+entries : actorNonZero(pc+1)))
      if (*f == p)
        goto found;
      else
        f++;
    GECODE_NEVER;
    found: ;
#endif
  }

  template <class VIC>
  forceinline void
  VarImp<VIC>::enter(Space& home, Advisor* a) {
    // Count one new subscription
    home.pc.p.n_sub += 1;
    if ((free_and_bits >> free_bits) == 0)
      resize(home);
    free_and_bits -= 1 << free_bits;

    // Enter subscription
    base[entries++] = *actorNonZero(pc_max+1);
    *actorNonZero(pc_max+1) = a;
  }

  template <class VIC>
  void
  VarImp<VIC>::resize(Space& home) {
    if (base == NULL) {
      assert((free_and_bits >> free_bits) == 0);
      // Create fresh dependency array with four entries
      free_and_bits += 4 << free_bits;
      base = home.alloc<ActorLink*>(4);
    } else {
      // Resize dependency array
      unsigned int n = degree();
      // Find out whether the area is most likely in the special area
      // reserved for subscriptions. If yes, just resize mildly otherwise
      // more agressively
      ActorLink** s = static_cast<ActorLink**>(home.mm.subscriptions());
      unsigned int m = 
        ((s <= base) && (base < s+home.pc.p.n_sub)) ?
        (n+4) : ((n+1)*3>>1);
      ActorLink** prop = home.alloc<ActorLink*>(m);
      free_and_bits += (m-n) << free_bits;
      // Copy entries
      Heap::copy<ActorLink*>(prop, base, n);
      home.free<ActorLink*>(base,n);
      base = prop;
    }
  }

  template <class VIC>
  void
  VarImp<VIC>::subscribe(Space& home, Propagator& p, PropCond pc,
                         bool assigned, ModEvent me, bool schedule) {
    if (assigned) {
      // Do not subscribe, just schedule the propagator
      if (schedule)
        VarImp<VIC>::schedule(home,p,ME_GEN_ASSIGNED);
    } else {
      enter(home,&p,pc);
      // Schedule propagator
      if (schedule && (pc != PC_GEN_ASSIGNED))
        VarImp<VIC>::schedule(home,p,me);
    }
  }

  template <class VIC>
  forceinline void
  VarImp<VIC>::subscribe(Space& home, Advisor& a, bool assigned) {
    if (!assigned)
      enter(home,&a);
  }

  template <class VIC>
  forceinline void
  VarImp<VIC>::remove(Space& home, Propagator* p, PropCond pc) {
    assert(pc <= pc_max);
    ActorLink* a = ActorLink::cast(p);
    // Find actor in dependency array
    ActorLink** f = actor(pc);
#ifdef GECODE_AUDIT
    while (f < actorNonZero(pc+1))
      if (*f == a)
        goto found;
      else
        f++;
    GECODE_NEVER;
  found: ;
#else
    while (*f != a) f++;
#endif
    // Remove actor
    *f = *(actorNonZero(pc+1)-1);
    for (PropCond j = pc+1; j< pc_max+1; j++) {
      *(actorNonZero(j)-1) = *(actorNonZero(j+1)-1);
      idx(j)--;
    }
    *(actorNonZero(pc_max+1)-1) = base[entries-1];
    idx(pc_max+1)--;
    entries--;
    free_and_bits += 1 << free_bits;
    home.pc.p.n_sub -= 1;
  }

  template <class VIC>
  forceinline void
  VarImp<VIC>::remove(Space& home, Advisor* a) {
    // Find actor in dependency array
    ActorLink** f = actorNonZero(pc_max+1);
#ifdef GECODE_AUDIT
    while (f < base+entries)
      if (*f == a)
        goto found;
      else
        f++;
    GECODE_NEVER;
  found: ;
#else
    while (*f != a) f++;
#endif
    // Remove actor
    *f = base[--entries];
    free_and_bits += 1 << free_bits;
    home.pc.p.n_sub -= 1;
  }

  template <class VIC>
  forceinline void
  VarImp<VIC>::cancel(Space& home, Propagator& p, PropCond pc, bool assigned) {
    if (!assigned)
      remove(home,&p,pc);
  }

  template <class VIC>
  forceinline void
  VarImp<VIC>::cancel(Space& home, Advisor& a, bool assigned) {
    if (!assigned)
      remove(home,&a);
  }

  template <class VIC>
  forceinline void
  VarImp<VIC>::cancel(Space& home) {
    // Entries in index structure are disabled. However they
    // must still work for cloning (base) and degree (idx(pc_max+2))
    unsigned int n_sub = degree();
    home.pc.p.n_sub -= n_sub;
    unsigned int n = (free_and_bits >> VIC::free_bits) + n_sub;
    home.free<ActorLink*>(base,n);
    base = NULL;
    entries = 0;
  }

  template <class VIC>
  forceinline bool
  VarImp<VIC>::advise(Space& home, ModEvent me, Delta& d) {
    /*
     * An advisor that is executed might remove itself due to subsumption.
     * As entries are removed from front to back, the advisors must
     * be iterated in forward direction.
     */
    ActorLink** la = actorNonZero(pc_max+1);
    ActorLink** le = base+entries;
    if (la == le)
      return true;
    d.me = me;
    // An advisor that is run, might be removed during execution.
    // As removal is done from the back the advisors have to be executed
    // in inverse order.
    do {
      Advisor* a = Advisor::cast(*la);
      assert(!a->disposed());
      Propagator& p = a->propagator();
      switch (p.advise(home,*a,d)) {
      case ES_FIX:
        break;
      case ES_FAILED:
        return false;
      case ES_NOFIX:
        schedule(home,p,me);
        break;
      default:
        GECODE_NEVER;
      }
    } while (++la < le);
    return true;
  }

  template <class VIC>
  forceinline void
  VarImp<VIC>::update(VarImp<VIC>* x, ActorLink**& sub) {
    // this refers to the variable to be updated (clone)
    // x refers to the original
    // Recover from copy
    x->base = base;
    x->u.idx[0] = u.idx[0];
    if (pc_max > 0 && sizeof(ActorLink**) > sizeof(unsigned int))
      x->u.idx[1] = u.idx[1];

    ActorLink** f = x->base;
    unsigned int n = x->degree();
    ActorLink** t = sub;
    sub += n;
    base = t;
    // Set subscriptions using actor forwarding pointers
    while (n >= 4) {
      n -= 4;
      t[0]=f[0]->prev(); t[1]=f[1]->prev();
      t[2]=f[2]->prev(); t[3]=f[3]->prev();
      t += 4; f += 4;
    }
    if (n >= 2) {
      n -= 2;
      t[0]=f[0]->prev(); t[1]=f[1]->prev();
      t += 2; f += 2;
    }
    if (n > 0) {
      t[0]=f[0]->prev();
    }
  }

  template <class VIC>
  forceinline void
  VarImp<VIC>::update(Space& home, ActorLink**& sub) {
    VarImp<VIC>* x = static_cast<VarImp<VIC>*>(home.pc.c.vars_u[idx_c]);
    while (x != NULL) {
      VarImp<VIC>* n = x->next(); x->forward()->update(x,sub); x = n;
    }
  }



  /*
   * Variable disposer
   *
   */
  template <class VarType>
  VarDisposer<VarType>::VarDisposer(void) {
#ifdef GECODE_HAS_VAR_DISPOSE
    Space::vd[VarType::idx_d] = this;
#endif
  }

  template <class VarType>
  void
  VarDisposer<VarType>::dispose(Space& home, VarImpBase* _x) {
    VarType* x = static_cast<VarType*>(_x);
    do {
      x->dispose(home); x = static_cast<VarType*>(x->next_d());
    } while (x != NULL);
  }


  forceinline SpaceStatus
  Space::status(unsigned long int& pn, bool& wmp) {
    SpaceStatus s = _status(pn);
    wmp = (n_wmp > 0);
    if (n_wmp == 1) n_wmp = 0;
    return s;
  }

}

// STATISTICS: kernel-core
