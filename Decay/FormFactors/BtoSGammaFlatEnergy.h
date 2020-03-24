// -*- C++ -*-
//
// BtoSGammaFlatEnergy.h is a part of Herwig - A multi-purpose Monte Carlo event generator
// Copyright (C) 2002-2019 The Herwig Collaboration
//
// Herwig is licenced under version 3 of the GPL, see COPYING for details.
// Please respect the MCnet academic guidelines, see GUIDELINES for details.
//
#ifndef HERWIG_BtoSGammaFlatEnergy_H
#define HERWIG_BtoSGammaFlatEnergy_H
//
// This is the declaration of the BtoSGammaFlatEnergy class.
//

#include "BtoSGammaHadronicMass.h"

namespace Herwig {

using namespace ThePEG;

/** \ingroup Decay
 *
 * The BtoSGammaFlatEnergy class is a model of the hadronic mass 
 * is \f$B\to s\gamma\f$ decays
 * which produces a flat photon energy spectrum and as such is only intended for
 * testing purposes.
 *
 * @see BtoSGammaHadronicMass
 */
class BtoSGammaFlatEnergy: public BtoSGammaHadronicMass {

public:

  /**
   * Return the hadronic mass.
   * @param mb The mass of the decaying B meson.
   * @param mquark The minimum mass of the hadronic system based on the consistuent quark
   * masses.
   * @return The hadronic mass
   */
  virtual Energy hadronicMass(Energy mb, Energy mquark);

  /**
   * Output the setup information for the particle database
   * @param os The stream to output the information to
   * @param header Whether or not to output the information for MySQL
   * @param create Whether or not to add a statement creating the object
   */
  virtual void dataBaseOutput(ofstream & os,bool header,bool create) const;

public:

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
  virtual IBPtr clone() const {return new_ptr(*this);}

  /** Make a clone of this object, possibly modifying the cloned object
   * to make it sane.
   * @return a pointer to the new object.
   */
  virtual IBPtr fullclone() const {return new_ptr(*this);}
  //@}

private:

  /**
   * The assignment operator is private and must never be called.
   * In fact, it should not even be implemented.
   */
  BtoSGammaFlatEnergy & operator=(const BtoSGammaFlatEnergy &) = delete;

};

}

#endif /* HERWIG_BtoSGammaFlatEnergy_H */
