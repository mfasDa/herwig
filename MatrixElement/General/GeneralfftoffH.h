// -*- C++ -*-
#ifndef HERWIG_GeneralfftoffH_H
#define HERWIG_GeneralfftoffH_H
//
// This is the declaration of the GeneralfftoffH class.
//

#include "Herwig/MatrixElement/MEfftoffH.h"
#include "GeneralfftoffH.fh"

namespace Herwig {

using namespace ThePEG;

/**
 * Here is the documentation of the GeneralfftoffH class.
 *
 * @see \ref GeneralfftoffHInterfaces "The interfaces"
 * defined for GeneralfftoffH.
 */
class GeneralfftoffH: public MEfftoffH {

public:

  /**
   *  Type of process
   */
  enum Process {Lepton,Hadron};

public:

  /**
   * The default constructor.
   */
  GeneralfftoffH();

  /** @name Virtual functions required by the MEBase class. */
  //@{
  /**
   * Add all possible diagrams with the add() function.
   */
  virtual void getDiagrams() const;
  //@}

  /**
   *  Set up the matrix element
   */
  void setProcessInfo(Process proc, PDPtr higgs,
		      AbstractVVSVertexPtr vertex,
		      unsigned int shapeOpt,
		      unsigned int process);

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

private:

  /**
   * The assignment operator is private and must never be called.
   * In fact, it should not even be implemented.
   */
  GeneralfftoffH & operator=(const GeneralfftoffH &) = delete;

private:

  /**
   *  The type of process
   */
  Process _proc;

};

}

#endif /* HERWIG_GeneralfftoffH_H */
