// -*- C++ -*-
//
// UEDF1F1Z0Vertex.h is a part of Herwig - A multi-purpose Monte Carlo event generator
// Copyright (C) 2002-2019 The Herwig Collaboration
//
// Herwig is licenced under version 3 of the GPL, see COPYING for details.
// Please respect the MCnet academic guidelines, see GUIDELINES for details.
//
#ifndef HERWIG_UEDF1F1Z0Vertex_H
#define HERWIG_UEDF1F1Z0Vertex_H
//
// This is the declaration of the UEDF1F1Z0Vertex class.
//

#include "ThePEG/Helicity/Vertex/Vector/FFVVertex.h"
#include "UEDBase.h"

namespace Herwig {
using namespace ThePEG;

/**
 * This is the implementation of the level-1 fermion-fermion Z-boson.
 * It inherits from FFVVertex and implements the setCoupling member.
 *
 * @see \ref UEDF1F1Z0VertexInterfaces "The interfaces"
 * defined for UEDF1F1Z0Vertex.
 */
class UEDF1F1Z0Vertex: public FFVVertex {

public:

  /**
   * The default constructor.
   */
  UEDF1F1Z0Vertex();

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

public:

  /** Calculate the coupling
   *@param q2 The scale at which to evaluate the coupling
   *@param part1 The first interacting particle 
   *@param part2 The second interacting particle 
   *@param part3 The third interacting particle 
   */
  virtual void setCoupling(Energy2 q2, tcPDPtr part1, tcPDPtr part2,
			   tcPDPtr part3);

protected:

  /** @name Clone Methods. */
  //@{
  /**
   * Make a simple clone of this object.
   * @return a pointer to the new object.
   */
  virtual IBPtr clone() const {return new_ptr(*this);}

  /** Make a clone of this object, possibly modifying the cloned object
   * to make it sane.
   * @return a pointer to the new object.
   */
  virtual IBPtr fullclone() const {return new_ptr(*this);}
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
  UEDF1F1Z0Vertex & operator=(const UEDF1F1Z0Vertex &) = delete;
  
private:

  /**
   * The value if \f$\sin^2\theta_W\f$
   */
  double theSin2ThW;

  /**
   * The value if \f$\cos\theta_W\f$
   */
  double theCosThW;

  /**
   * The value of the inverse radius
   */
  InvEnergy theRadius;

  /**
   * The PDG code of the 1st fermion when the vertex was last evaluated
   */
  long theID1Last;

  /**
   * The PDG code of the 2nd fermion when the vertex was last evaluated
   */
  long theID2Last;

  /**
   * The energy where the coupling was last evaluated
   */
  Energy2 theq2Last;

  /**
   *
   * The value of the coupling when it was last evaluated
   */
  Complex theCoupLast;

  /**
   *
   * The value of the coupling when it was last evaluated
   */
  Complex theLeftLast;

  /**
   *
   * The value of the coupling when it was last evaluated
   */
  Complex theRightLast;  
};
}

#endif /* HERWIG_UEDF1F1Z0Vertex_H */
