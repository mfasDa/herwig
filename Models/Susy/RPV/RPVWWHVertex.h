// -*- C++ -*-
#ifndef Herwig_RPVWWHVertex_H
#define Herwig_RPVWWHVertex_H
//
// This is the declaration of the RPVWWHVertex class.
//

#include "ThePEG/Helicity/Vertex/Scalar/VVSVertex.h"

namespace Herwig {

using namespace ThePEG;

/**
 * The RPVWWHVertex class implements the couplings of a pair of gauge bosons to scalars in 
 * the R-parity violating MSSM.
 */
class RPVWWHVertex: public Helicity::VVSVertex {

public:

  /**
   * The default constructor.
   */
  RPVWWHVertex();

  /**
   * Calculate the coupling for the vertex
   * @param q2 The scale to at which evaluate the coupling.
   * @param particle1 The first particle in the vertex.
   * @param particle2 The second particle in the vertex.
   * @param particle3 The third particle in the vertex.
   */
  virtual void setCoupling(Energy2 q2, tcPDPtr particle1, tcPDPtr particle2,
			   tcPDPtr particle3);

public:

  /** @name Functions used by the persistent I/O system. */
  //@{
  /**
   * Function used to write out object persistently.
   * @param os the persistent output stream written to.
   */
  void persistentOutput(PersistentOStream & os) const;

  /**
   * Function used to read in object persistently.
   * @param is the persistent input stream read from.
   * @param version the version number of the object when written.
   */
  void persistentInput(PersistentIStream & is, int version);
  //@}

  /**
   * The standard Init function used to initialize the interfaces.
   * Called exactly once for each class by the class description system
   * before the main function starts or
   * when this class is dynamically loaded.
   */
  static void Init();

protected:

  /** @name Clone Methods. */
  //@{
  /**
   * Make a simple clone of this object.
   * @return a pointer to the new object.
   */
  virtual IBPtr clone() const;

  /** Make a clone of this object, possibly modifying the cloned object
   * to make it sane.
   * @return a pointer to the new object.
   */
  virtual IBPtr fullclone() const;
  //@}

protected:

  /** @name Standard Interfaced functions. */
  //@{
  /**
   * Initialize this object after the setup phase before saving an
   * EventGenerator to disk.
   * @throws InitException if object could not be initialized properly.
   */
  virtual void doinit();
  //@}

private:

  /**
   * The assignment operator is private and must never be called.
   * In fact, it should not even be implemented.
   */
  RPVWWHVertex & operator=(const RPVWWHVertex &) = delete;

private:

  /**
   *  The factors for the various interactions
   */
  vector<vector<complex<Energy> > > couplings_; 

  /**
   *  last value of the coupling
   */
  double coupLast_;

  /**
   *  last value of the scale
   */
  Energy2 q2Last_;

};

}

#endif /* Herwig_RPVWWHVertex_H */
