// -*- C++ -*-
//
// MatchboxMEBase.cc is a part of Herwig - A multi-purpose Monte Carlo event generator
// Copyright (C) 2002-2012 The Herwig Collaboration
//
// Herwig is licenced under version 2 of the GPL, see COPYING for details.
// Please respect the MCnet academic guidelines, see GUIDELINES for details.
//
//
// This is the implementation of the non-inlined, non-templated member
// functions of the MatchboxMEBase class.
//

#include "MatchboxMEBase.h"
#include "ThePEG/Utilities/DescribeClass.h"
#include "ThePEG/Interface/ClassDocumentation.h"
#include "ThePEG/Interface/Parameter.h"
#include "ThePEG/Interface/Reference.h"
#include "ThePEG/Interface/RefVector.h"
#include "ThePEG/Interface/Switch.h"
#include "ThePEG/Persistency/PersistentOStream.h"
#include "ThePEG/Persistency/PersistentIStream.h"
#include "ThePEG/PDF/PDF.h"
#include "ThePEG/PDT/PDT.h"
#include "ThePEG/StandardModel/StandardModelBase.h"
#include "ThePEG/Cuts/Cuts.h"
#include "ThePEG/Handlers/StdXCombGroup.h"
#include "ThePEG/EventRecord/SubProcess.h"
#include "Herwig/MatrixElement/Matchbox/Dipoles/SubtractionDipole.h"
#include "Herwig/MatrixElement/Matchbox/Utility/DiagramDrawer.h"
#include "Herwig/MatrixElement/Matchbox/MatchboxFactory.h"
#include "Herwig/API/RunDirectories.h"
#include "Herwig/MatrixElement/ProductionMatrixElement.h"
#include "Herwig/MatrixElement/HardVertex.h"

#include <boost/foreach.hpp>
#include <cctype>

#include <iterator>
using std::ostream_iterator;

using namespace Herwig;

MatchboxMEBase::MatchboxMEBase() 
  : MEBase(), 
    theOneLoop(false),
    theOneLoopNoBorn(false),
    theOneLoopNoLoops(false),
    theNoCorrelations(false),
    theHavePDFs(false,false), checkedPDFs(false),
    theDiagramWeightVerboseDown(10000000000000.),
    theDiagramWeightVerboseUp(0.) {}

MatchboxMEBase::~MatchboxMEBase() {}

Ptr<MatchboxFactory>::tptr MatchboxMEBase::factory() const { return theFactory; }

void MatchboxMEBase::factory(Ptr<MatchboxFactory>::tptr f) { theFactory = f; }

Ptr<Tree2toNGenerator>::tptr MatchboxMEBase::diagramGenerator() const { return factory()->diagramGenerator(); }

Ptr<ProcessData>::tptr MatchboxMEBase::processData() const { return factory()->processData(); }

unsigned int MatchboxMEBase::getNLight() const { return factory()->nLight(); }

vector<int> MatchboxMEBase::getNLightJetVec() const { return factory()->nLightJetVec(); }

vector<int> MatchboxMEBase::getNHeavyJetVec() const { return factory()->nHeavyJetVec(); }

vector<int> MatchboxMEBase::getNLightProtonVec() const { return factory()->nLightProtonVec(); }

double MatchboxMEBase::factorizationScaleFactor() const { return factory()->factorizationScaleFactor(); }

double MatchboxMEBase::renormalizationScaleFactor() const { return factory()->renormalizationScaleFactor(); }

bool MatchboxMEBase::fixedCouplings() const { return factory()->fixedCouplings(); }

bool MatchboxMEBase::fixedQEDCouplings() const { return factory()->fixedQEDCouplings(); }

bool MatchboxMEBase::checkPoles() const { return factory()->checkPoles(); }

bool MatchboxMEBase::verbose() const { return factory()->verbose(); }

bool MatchboxMEBase::initVerbose() const { return factory()->initVerbose(); }

void MatchboxMEBase::getDiagrams() const {

  if ( diagramGenerator() && processData() ) {

    vector<Ptr<Tree2toNDiagram>::ptr> diags;

    vector<Ptr<Tree2toNDiagram>::ptr>& res =
      processData()->diagramMap()[subProcess().legs];
    if ( res.empty() ) {
      res = diagramGenerator()->generate(subProcess().legs,orderInAlphaS(),orderInAlphaEW());
    }
    copy(res.begin(),res.end(),back_inserter(diags));
    processData()->fillMassGenerators(subProcess().legs);

    if ( diags.empty() )
      return;

    for ( vector<Ptr<Tree2toNDiagram>::ptr>::iterator d = diags.begin();
	  d != diags.end(); ++d ) {
      add(*d);
    }

    return;

  }

  throw Exception()
    << "MatchboxMEBase::getDiagrams() expects a Tree2toNGenerator and ProcessData object.\n"
    << "Please check your setup." << Exception::runerror;

}

Selector<MEBase::DiagramIndex> 
MatchboxMEBase::diagrams(const DiagramVector & diags) const {

  if ( phasespace() ) {
    return phasespace()->selectDiagrams(diags);
  }

  throw Exception()
    << "MatchboxMEBase::diagrams() expects a MatchboxPhasespace object.\n"
    << "Please check your setup." << Exception::runerror;
  return Selector<MEBase::DiagramIndex>();

}

Selector<const ColourLines *>
MatchboxMEBase::colourGeometries(tcDiagPtr diag) const {

  if ( matchboxAmplitude() ) {
    if ( matchboxAmplitude()->haveColourFlows() ) {
      if ( matchboxAmplitude()->treeAmplitudes() )
	matchboxAmplitude()->prepareAmplitudes(this);
      return matchboxAmplitude()->colourGeometries(diag);
    }
  }

  Ptr<Tree2toNDiagram>::tcptr tdiag =
    dynamic_ptr_cast<Ptr<Tree2toNDiagram>::tcptr>(diag);
  assert(diag && processData());

  vector<ColourLines*>& flows = processData()->colourFlowMap()[tdiag];

  if ( flows.empty() ) {

    list<list<list<pair<int,bool> > > > cflows =
      ColourBasis::colourFlows(tdiag);

    for ( list<list<list<pair<int,bool> > > >::const_iterator fit =
	    cflows.begin(); fit != cflows.end(); ++fit ) {
      flows.push_back(new ColourLines(ColourBasis::cfstring(*fit)));
    }

  }

  Selector<const ColourLines *> res;
  for ( vector<ColourLines*>::const_iterator f = flows.begin();
	f != flows.end(); ++f )
    res.insert(1.0,*f);

  return res;

}

void MatchboxMEBase::constructVertex(tSubProPtr sub, const ColourLines* cl) {

  if ( !canFillRhoMatrix() || !factory()->spinCorrelations() )
    return;

  assert(matchboxAmplitude());
  assert(matchboxAmplitude()->colourBasis());

  // get the colour structure for the selected colour flow
  size_t cStructure = 
    matchboxAmplitude()->colourBasis()->tensorIdFromFlow(lastXComb().lastDiagram(),cl);

  // hard process for processing the spin info
  tPVector hard;
  hard.push_back(sub->incoming().first);
  hard.push_back(sub->incoming().second);
  
  vector<PDT::Spin> out;
  for ( size_t k = 0; k < sub->outgoing().size(); ++k ) {
    out.push_back(sub->outgoing()[k]->data().iSpin());
    hard.push_back(sub->outgoing()[k]);
  }

  // calculate dummy wave functions to fill the spin info
  static vector<VectorWaveFunction> dummyPolarizations;
  static vector<SpinorWaveFunction> dummySpinors;
  static vector<SpinorBarWaveFunction> dummyBarSpinors;
  for ( size_t k = 0; k < hard.size(); ++k ) {
    if ( hard[k]->data().iSpin() == PDT::Spin1Half ) {
      if ( hard[k]->id() > 0 && k > 1 ) {
	SpinorBarWaveFunction(dummyBarSpinors,hard[k],
			      outgoing, true);
      } else if ( hard[k]->id() < 0 && k > 1 ) {
	SpinorWaveFunction(dummySpinors,hard[k],
			   outgoing, true);
      } else if ( hard[k]->id() > 0 && k < 2 ) {
	SpinorWaveFunction(dummySpinors,hard[k],
			   incoming, false);
      } else if ( hard[k]->id() < 0 && k < 2 ) {
	SpinorBarWaveFunction(dummyBarSpinors,hard[k],
			      incoming, false);
      }
    } 
    else if ( hard[k]->data().iSpin() == PDT::Spin1 ) {
      VectorWaveFunction(dummyPolarizations,hard[k],
			 k > 1 ? outgoing : incoming,
			 k > 1 ? true : false,
			 hard[k]->data().hardProcessMass() == ZERO);
    }
    else if (hard[k]->data().iSpin() == PDT::Spin0 ) {
      ScalarWaveFunction(hard[k],k > 1 ? outgoing : incoming,
			 k > 1 ? true : false);
    }
    else
      assert(false);
  }

  // fill the production matrix element
  ProductionMatrixElement pMe(mePartonData()[0]->iSpin(),
			      mePartonData()[1]->iSpin(),
			      out);
  for ( map<vector<int>,CVector>::const_iterator lamp = lastLargeNAmplitudes().begin();
	lamp != lastLargeNAmplitudes().end(); ++lamp ) {
    vector<unsigned int> pMeHelicities
      = matchboxAmplitude()->physicalHelicities(lamp->first);
    pMe(pMeHelicities) = lamp->second[cStructure];
  }

  // set the spin information
  HardVertexPtr hardvertex = new_ptr(HardVertex());
  hardvertex->ME(pMe);
  if ( sub->incoming().first->spinInfo() )
    sub->incoming().first->spinInfo()->productionVertex(hardvertex);
  if ( sub->incoming().second->spinInfo() )
    sub->incoming().second->spinInfo()->productionVertex(hardvertex);
  for ( ParticleVector::const_iterator p = sub->outgoing().begin();
	p != sub->outgoing().end(); ++p ) {
    if ( (**p).spinInfo() )
      (**p).spinInfo()->productionVertex(hardvertex);
  }

}

unsigned int MatchboxMEBase::orderInAlphaS() const {
  return subProcess().orderInAlphaS;
}

unsigned int MatchboxMEBase::orderInAlphaEW() const {
  return subProcess().orderInAlphaEW;
}

void MatchboxMEBase::setXComb(tStdXCombPtr xc) {

  MEBase::setXComb(xc);
  lastMatchboxXComb(xc);
  if ( phasespace() )
    phasespace()->setXComb(xc);
  if ( scaleChoice() )
    scaleChoice()->setXComb(xc);
  if ( matchboxAmplitude() )
    matchboxAmplitude()->setXComb(xc);

}

double MatchboxMEBase::generateIncomingPartons(const double* r1, const double* r2) {
  
  // shamelessly stolen from PartonExtractor.cc

  Energy2 shmax = lastCuts().sHatMax();
  Energy2 shmin = lastCuts().sHatMin();
  Energy2 sh = shmin*pow(shmax/shmin, *r1);
  double ymax = lastCuts().yHatMax();
  double ymin = lastCuts().yHatMin();
  double km = log(shmax/shmin);
  ymax = min(ymax, log(lastCuts().x1Max()*sqrt(lastS()/sh)));
  ymin = max(ymin, -log(lastCuts().x2Max()*sqrt(lastS()/sh)));

  double y = ymin + (*r2)*(ymax - ymin);
  double x1 = exp(-0.5*log(lastS()/sh) + y);
  double x2 = exp(-0.5*log(lastS()/sh) - y);

  Lorentz5Momentum P1 = lastParticles().first->momentum();
  LorentzMomentum p1 = lightCone((P1.rho() + P1.e())*x1, Energy());
  p1.rotateY(P1.theta());
  p1.rotateZ(P1.phi());
  meMomenta()[0] = p1;

  Lorentz5Momentum P2 = lastParticles().second->momentum();
  LorentzMomentum p2 = lightCone((P2.rho() + P2.e())*x2, Energy());
  p2.rotateY(P2.theta());
  p2.rotateZ(P2.phi());
  meMomenta()[1] = p2;

  lastXCombPtr()->lastX1X2(make_pair(x1,x2));
  lastXCombPtr()->lastSHat((meMomenta()[0]+meMomenta()[1]).m2());

  return km*(ymax - ymin);

}

bool MatchboxMEBase::generateKinematics(const double * r) {

  if ( phasespace() ) {

    jacobian(phasespace()->generateKinematics(r,meMomenta()));
    if ( jacobian() == 0.0 )
      return false;

    setScale();
    logGenerateKinematics(r);

    assert(lastMatchboxXComb());

    if ( nDimAmplitude() > 0 ) {
      amplitudeRandomNumbers().resize(nDimAmplitude());
      copy(r + nDimPhasespace(), 
	   r + nDimPhasespace() + nDimAmplitude(),
	   amplitudeRandomNumbers().begin());
    }

    if ( nDimInsertions() > 0 ) {
      insertionRandomNumbers().resize(nDimInsertions());
      copy(r + nDimPhasespace() + nDimAmplitude(), 
	   r + nDimPhasespace() + nDimAmplitude() + nDimInsertions(),
	   insertionRandomNumbers().begin());
    }

    return true;

  }

  throw Exception()
    << "MatchboxMEBase::generateKinematics() expects a MatchboxPhasespace object.\n"
    << "Please check your setup." << Exception::runerror;

  return false;

}

int MatchboxMEBase::nDim() const { 

  if ( lastMatchboxXComb() )
    return nDimPhasespace() + nDimAmplitude() + nDimInsertions();

  int ampAdd = 0;
  if ( matchboxAmplitude() ) {
    ampAdd = matchboxAmplitude()->nDimAdditional();
  }

  int insertionAdd = 0;
  for ( vector<Ptr<MatchboxInsertionOperator>::ptr>::const_iterator v =
	  virtuals().begin(); v != virtuals().end(); ++v ) {
    insertionAdd = max(insertionAdd,(**v).nDimAdditional());
  }

  return nDimBorn() + ampAdd + insertionAdd;

}

int MatchboxMEBase::nDimBorn() const { 

  if ( lastMatchboxXComb() )
    return nDimPhasespace();

  if ( phasespace() )
    return phasespace()->nDim(diagrams().front()->partons());

  throw Exception()
    << "MatchboxMEBase::nDim() expects a MatchboxPhasespace object.\n"
    << "Please check your setup." << Exception::runerror;

  return 0;

}

void MatchboxMEBase::setScale() const {
  if ( haveX1X2() ) {
    lastXCombPtr()->lastSHat((meMomenta()[0]+meMomenta()[1]).m2());
  }
  Energy2 fcscale = factorizationScale();
  Energy2 fscale = fcscale*sqr(factorizationScaleFactor());
  Energy2 rscale = renormalizationScale()*sqr(renormalizationScaleFactor());
  Energy2 ewrscale = renormalizationScaleQED();
  lastXCombPtr()->lastScale(fscale);
  lastXCombPtr()->lastCentralScale(fcscale);
  lastXCombPtr()->lastShowerScale(showerScale());
  lastMatchboxXComb()->lastRenormalizationScale(rscale);
  if ( !fixedCouplings() ) {
    if ( rscale > lastCuts().scaleMin() )
      lastXCombPtr()->lastAlphaS(SM().alphaS(rscale));
    else
      lastXCombPtr()->lastAlphaS(SM().alphaS(lastCuts().scaleMin()));
  } else {
    lastXCombPtr()->lastAlphaS(SM().alphaS());
  }
  if ( !fixedQEDCouplings() ) {
    lastXCombPtr()->lastAlphaEM(SM().alphaEMME(ewrscale));
  } else {
    lastXCombPtr()->lastAlphaEM(SM().alphaEMMZ());
  }
  logSetScale();
}

Energy2 MatchboxMEBase::factorizationScale() const {
  if ( scaleChoice() ) {
    return scaleChoice()->factorizationScale();
  }

  throw Exception()
    << "MatchboxMEBase::factorizationScale() expects a MatchboxScaleChoice object.\n"
    << "Please check your setup." << Exception::runerror;

  return ZERO;

}

Energy2 MatchboxMEBase::renormalizationScale() const {
  if ( scaleChoice() ) {
    return scaleChoice()->renormalizationScale();
  }

  throw Exception()
    << "MatchboxMEBase::renormalizationScale() expects a MatchboxScaleChoice object.\n"
    << "Please check your setup." << Exception::runerror;

  return ZERO;

}

Energy2 MatchboxMEBase::renormalizationScaleQED() const {
  if ( scaleChoice() ) {
    return scaleChoice()->renormalizationScaleQED();
  }
  return renormalizationScale();
}

Energy2 MatchboxMEBase::showerScale() const {
  if ( scaleChoice() ) {
    return scaleChoice()->showerScale();
  }

  throw Exception()
    << "MatchboxMEBase::showerScale() expects a MatchboxScaleChoice object.\n"
    << "Please check your setup." << Exception::runerror;

  return ZERO;

}

void MatchboxMEBase::setVetoScales(tSubProPtr) const {}

bool MatchboxMEBase::havePDFWeight1() const {
  if ( checkedPDFs )
    return theHavePDFs.first;
  theHavePDFs.first = 
    factory()->isIncoming(mePartonData()[0]) && 
    lastXCombPtr()->partonBins().first->pdf();
  theHavePDFs.second = 
    factory()->isIncoming(mePartonData()[1]) &&
    lastXCombPtr()->partonBins().second->pdf();
  checkedPDFs = true;
  return theHavePDFs.first;
}

bool MatchboxMEBase::havePDFWeight2() const {
  if ( checkedPDFs )
    return theHavePDFs.second;
  theHavePDFs.first = 
    factory()->isIncoming(mePartonData()[0]) && 
    lastXCombPtr()->partonBins().first->pdf();
  theHavePDFs.second = 
    factory()->isIncoming(mePartonData()[1]) &&
    lastXCombPtr()->partonBins().second->pdf();
  checkedPDFs = true;
  return theHavePDFs.second;
}

void MatchboxMEBase::getPDFWeight(Energy2 factorizationScale) const {

  if ( !havePDFWeight1() && !havePDFWeight2() ) {
    lastMEPDFWeight(1.0);
    logPDFWeight();
    return;
  }

  double w = 1.;

  if ( havePDFWeight1() )
    w *= pdf1(factorizationScale);

  if ( havePDFWeight2() )
    w *= pdf2(factorizationScale);

  lastMEPDFWeight(w);

  logPDFWeight();

}

double MatchboxMEBase::pdf1(Energy2 fscale, double xEx, double xFactor) const {

  assert(lastXCombPtr()->partonBins().first->pdf());

  if ( xEx < 1. && lastX1()*xFactor >= xEx ) {
    return
      ( ( 1. - lastX1()*xFactor ) / ( 1. - xEx ) ) *
      lastXCombPtr()->partonBins().first->pdf()->xfx(lastParticles().first->dataPtr(),
						     lastPartons().first->dataPtr(),
						     fscale == ZERO ? lastScale() : fscale,
						     xEx)/xEx;
  }

  return lastXCombPtr()->partonBins().first->pdf()->xfx(lastParticles().first->dataPtr(),
							lastPartons().first->dataPtr(),
							fscale == ZERO ? lastScale() : fscale,
							lastX1()*xFactor)/lastX1()/xFactor;
}

double MatchboxMEBase::pdf2(Energy2 fscale, double xEx, double xFactor) const {

  assert(lastXCombPtr()->partonBins().second->pdf());

  if ( xEx < 1. && lastX2()*xFactor >= xEx ) {
    return
      ( ( 1. - lastX2()*xFactor ) / ( 1. - xEx ) ) *
      lastXCombPtr()->partonBins().second->pdf()->xfx(lastParticles().second->dataPtr(),
						      lastPartons().second->dataPtr(),
						      fscale == ZERO ? lastScale() : fscale,
						      xEx)/xEx;
  }

  return lastXCombPtr()->partonBins().second->pdf()->xfx(lastParticles().second->dataPtr(),
							 lastPartons().second->dataPtr(),
							 fscale == ZERO ? lastScale() : fscale,
							 lastX2()*xFactor)/lastX2()/xFactor;

}

double MatchboxMEBase::me2() const {

  if ( matchboxAmplitude() ) {

    if ( matchboxAmplitude()->treeAmplitudes() )
      matchboxAmplitude()->prepareAmplitudes(this);

    double res = 
      matchboxAmplitude()->me2()*
      me2Norm();

    return res;

  }

  throw Exception()
    << "MatchboxMEBase::me2() expects a MatchboxAmplitude object.\n"
    << "Please check your setup." << Exception::runerror;
  return 0.;

}

double MatchboxMEBase::largeNME2(Ptr<ColourBasis>::tptr largeNBasis) const {

  if ( matchboxAmplitude() ) {

    if ( matchboxAmplitude()->treeAmplitudes() ) {
      largeNBasis->prepare(mePartonData(),false);
      matchboxAmplitude()->prepareAmplitudes(this);
    }

    double res = 
      matchboxAmplitude()->largeNME2(largeNBasis)*
      me2Norm();

    return res;

  }

  throw Exception()
    << "MatchboxMEBase::largeNME2() expects a MatchboxAmplitude object.\n"
    << "Please check your setup." << Exception::runerror;
  return 0.;

}

double MatchboxMEBase::finalStateSymmetry() const {

  if ( symmetryFactor() > 0.0 )
    return symmetryFactor();

  double sFactor = 1.;

  map<long,int> counts;

  cPDVector checkData;
  copy(mePartonData().begin()+2,mePartonData().end(),back_inserter(checkData));

  cPDVector::iterator p = checkData.begin();
  while ( !checkData.empty() ) {
    if ( counts.find((**p).id()) != counts.end() ) {
      counts[(**p).id()] += 1;
    } else {
      counts[(**p).id()] = 1;
    }
    checkData.erase(p);
    p = checkData.begin();
    continue;
  }

  for ( map<long,int>::const_iterator c = counts.begin();
	c != counts.end(); ++c ) {
    if ( c->second == 1 )
      continue;
    if ( c->second == 2 )
      sFactor /= 2.;
    else if ( c->second == 3 )
      sFactor /= 6.;
    else if ( c->second == 4 )
      sFactor /= 24.;
  }

  symmetryFactor(sFactor);

  return symmetryFactor();

}

double MatchboxMEBase::me2Norm(unsigned int addAlphaS) const {

  // assume that we always have incoming
  // spin-1/2 or massless spin-1 particles
  double fac = 1./4.;

  if ( hasInitialAverage() )
    fac = 1.;

  double couplings = 1.0;
  if ( (orderInAlphaS() > 0 || addAlphaS != 0) && !hasRunningAlphaS() ) {
    fac *= pow(lastAlphaS()/SM().alphaS(),double(orderInAlphaS()+addAlphaS));
    couplings *= pow(lastAlphaS(),double(orderInAlphaS()+addAlphaS));
  }
  if ( orderInAlphaEW() > 0 && !hasRunningAlphaEW() ) {
    fac *= pow(lastAlphaEM()/SM().alphaEMMZ(),double(orderInAlphaEW()));
    couplings *= pow(lastAlphaEM(),double(orderInAlphaEW()));
  }
  lastMECouplings(couplings);

  if ( !hasInitialAverage() ) {
    if ( mePartonData()[0]->iColour() == PDT::Colour3 || 
	 mePartonData()[0]->iColour() == PDT::Colour3bar )
      fac /= SM().Nc();
    else if ( mePartonData()[0]->iColour() == PDT::Colour8 )
      fac /= (SM().Nc()*SM().Nc()-1.);

    if ( mePartonData()[1]->iColour() == PDT::Colour3 || 
	 mePartonData()[1]->iColour() == PDT::Colour3bar )
      fac /= SM().Nc();
    else if ( mePartonData()[1]->iColour() == PDT::Colour8 )
      fac /= (SM().Nc()*SM().Nc()-1.);
  }

  return !hasFinalStateSymmetry() ? finalStateSymmetry()*fac : fac;

}

CrossSection MatchboxMEBase::dSigHatDR() const {

  getPDFWeight();

  if ( !lastXCombPtr()->willPassCuts() ) {
    lastMECrossSection(ZERO);
    return lastMECrossSection();
  }

  double xme2 = me2();
  
  if (factory()->verboseDia()){
    double diagweightsum = 0.0;
    for ( vector<Ptr<DiagramBase>::ptr>::const_iterator d = diagrams().begin();
        d != diagrams().end(); ++d ) {
        diagweightsum += phasespace()->diagramWeight(dynamic_cast<const Tree2toNDiagram&>(**d));
    }
    double piWeight = pow(2.*Constants::pi,(int)(3*(meMomenta().size()-2)-4));
    double units = pow(lastSHat() / GeV2, mePartonData().size() - 4.);
    bookMEoverDiaWeight(log(xme2/(diagweightsum*piWeight*units)));//
  }
  
  if ( xme2 == 0. && !oneLoopNoBorn() ) {
    lastMECrossSection(ZERO);
    return lastMECrossSection();
  }

  double vme2 = 0.;
  if ( oneLoop() && !oneLoopNoLoops() )
    vme2 = oneLoopInterference();

  CrossSection res = ZERO;

  if ( !oneLoopNoBorn() )
    res += 
      (sqr(hbarc)/(2.*lastSHat())) *
      jacobian()* lastMEPDFWeight() * xme2;

  if ( oneLoop() && !oneLoopNoLoops() )
    res += 
      (sqr(hbarc)/(2.*lastSHat())) *
      jacobian()* lastMEPDFWeight() * vme2;

  if ( !onlyOneLoop() ) {
    for ( vector<Ptr<MatchboxInsertionOperator>::ptr>::const_iterator v =
	    virtuals().begin(); v != virtuals().end(); ++v ) {
      (**v).setXComb(lastXCombPtr());
      res += (**v).dSigHatDR();
    }
    if ( checkPoles() && oneLoop() )
      logPoles();
  }

  double weight = 0.0;
  bool applied = false;
  for ( vector<Ptr<MatchboxReweightBase>::ptr>::const_iterator rw =
	  theReweights.begin(); rw != theReweights.end(); ++rw ) {
    (**rw).setXComb(lastXCombPtr());
    if ( !(**rw).apply() )
      continue;
    weight += (**rw).evaluate();
    applied = true;
  }
  if ( applied )
    res *= weight;
  lastMECrossSection(res);

  return lastMECrossSection();

}

double MatchboxMEBase::oneLoopInterference() const {

  if ( matchboxAmplitude() ) {

    if ( matchboxAmplitude()->oneLoopAmplitudes() )
      matchboxAmplitude()->prepareOneLoopAmplitudes(this);

    double res = 
      matchboxAmplitude()->oneLoopInterference()*
      me2Norm(1);

    return res;

  }

  throw Exception()
    << "MatchboxMEBase::oneLoopInterference() expects a MatchboxAmplitude object.\n"
    << "Please check your setup." << Exception::runerror;
  return 0.;

}

MatchboxMEBase::AccuracyHistogram::AccuracyHistogram(double low,
						     double up,
						     unsigned int nbins) 
  : lower(low), upper(up), 
    sameSign(0), oppositeSign(0), nans(0),
    overflow(0), underflow(0) {

  double step = (up-low)/nbins;

  for ( unsigned int k = 1; k <= nbins; ++k )
    bins[lower + k*step] = 0.0;

}

void MatchboxMEBase::AccuracyHistogram::book(double a, double b) {
  if ( std::isnan(a) || std::isnan(b) ||
       std::isinf(a) || std::isinf(b) ) {
    ++nans;
    return;
  }
  if ( a*b >= 0. )
    ++sameSign;
  if ( a*b < 0. )
    ++oppositeSign;
  double r = 1.;
  if ( abs(a) != 0.0 )
    r = abs(1.-abs(b/a));
  else if ( abs(b) != 0.0 )
    r = abs(b);
  if ( log10(r) < lower || r == 0.0 ) {
    ++underflow;
    return;
  }
  if ( log10(r) > upper ) {
    ++overflow;
    return;
  }
  map<double,double>::iterator bin =
    bins.upper_bound(log10(r));
  if ( bin == bins.end() )
    return;
  bin->second += 1.;
}

void MatchboxMEBase::AccuracyHistogram::dump(const std::string& folder, const std::string& prefix,
					     const cPDVector& proc) const {
  ostringstream fname("");
  for ( cPDVector::const_iterator p = proc.begin();
	p != proc.end(); ++p )
    fname << (**p).PDGName();
  ofstream out((folder+"/"+prefix+fname.str()+".dat").c_str());
  out << "# same sign : " << sameSign << " opposite sign : "
      << oppositeSign << " nans : " << nans 
      << " overflow : " << overflow
      << " underflow : " << underflow << "\n";
  for ( map<double,double>::const_iterator b = bins.begin();
	b != bins.end(); ++b ) {
    map<double,double>::const_iterator bp = b; --bp;
    if ( b->second != 0. ) {
      if ( b != bins.begin() )
	out << bp->first;
      else
	out << lower;
      out << " " << b->first
	  << " " << b->second
	  << "\n" << flush;
    }
  }
  ofstream gpout((folder+"/"+prefix+fname.str()+".gp").c_str());
  gpout << "set terminal png\n"
      << "set xlabel 'accuracy of pole cancellation [decimal places]'\n"
      << "set ylabel 'counts\n"
      << "set xrange [-20:0]\n"
      << "set output '" << prefix << fname.str() << ".png'\n"
      << "plot '" << prefix << fname.str() << ".dat' using (0.5*($1+$2)):3 with linespoints pt 7 ps 1 not";
}

void MatchboxMEBase::AccuracyHistogram::persistentOutput(PersistentOStream& os) const {
  os << lower << upper << bins
     << sameSign << oppositeSign << nans
     << overflow << underflow;
}

void MatchboxMEBase::AccuracyHistogram::persistentInput(PersistentIStream& is) {
  is >> lower >> upper >> bins
     >> sameSign >> oppositeSign >> nans
     >> overflow >> underflow;
}

void MatchboxMEBase::logPoles() const {
  double res2me = oneLoopDoublePole();
  double res1me = oneLoopSinglePole();
  double res2i = 0.;
  double res1i = 0.;
  for ( vector<Ptr<MatchboxInsertionOperator>::ptr>::const_iterator v =
	  virtuals().begin(); v != virtuals().end(); ++v ) {
    res2i += (**v).oneLoopDoublePole();
    res1i += (**v).oneLoopSinglePole();
  }
  if (res2me != 0.0 || res2i != 0.0) epsilonSquarePoleHistograms[mePartonData()].book(res2me,res2i);
  if (res1me != 0.0 || res1i != 0.0) epsilonPoleHistograms[mePartonData()].book(res1me,res1i);
}

bool MatchboxMEBase::haveOneLoop() const {
  if ( matchboxAmplitude() )
    return matchboxAmplitude()->haveOneLoop();
  return false;
}

bool MatchboxMEBase::onlyOneLoop() const {
  if ( matchboxAmplitude() )
    return matchboxAmplitude()->onlyOneLoop();
  return false;
}

bool MatchboxMEBase::isDRbar() const {
  if ( matchboxAmplitude() )
    return matchboxAmplitude()->isDRbar();
  return false;
}

bool MatchboxMEBase::isDR() const {
  if ( matchboxAmplitude() )
    return matchboxAmplitude()->isDR();
  return false;
}

bool MatchboxMEBase::isCS() const {
  if ( matchboxAmplitude() )
    return matchboxAmplitude()->isCS();
  return false;
}

bool MatchboxMEBase::isBDK() const {
  if ( matchboxAmplitude() )
    return matchboxAmplitude()->isBDK();
  return false;
}

bool MatchboxMEBase::isExpanded() const {
  if ( matchboxAmplitude() )
    return matchboxAmplitude()->isExpanded();
  return false;
}

Energy2 MatchboxMEBase::mu2() const {
  if ( matchboxAmplitude() )
    return matchboxAmplitude()->mu2();
  return 0*GeV2;
}

double MatchboxMEBase::oneLoopDoublePole() const {

  if ( matchboxAmplitude() ) {

    return
      matchboxAmplitude()->oneLoopDoublePole()*
      me2Norm(1);

  }

  return 0.;

}

double MatchboxMEBase::oneLoopSinglePole() const {

  if ( matchboxAmplitude() ) {

    return 
      matchboxAmplitude()->oneLoopSinglePole()*
      me2Norm(1);

  }

  return 0.;

}

vector<Ptr<SubtractionDipole>::ptr> 
MatchboxMEBase::getDipoles(const vector<Ptr<SubtractionDipole>::ptr>& dipoles,
			   const vector<Ptr<MatchboxMEBase>::ptr>& borns) const {

  vector<Ptr<SubtractionDipole>::ptr> res;

  // keep track of the dipoles we already did set up
  set<pair<pair<pair<int,int>,int>,pair<Ptr<MatchboxMEBase>::tptr,Ptr<SubtractionDipole>::tptr> > > done;

  cPDVector rep = diagrams().front()->partons();
  int nreal = rep.size();

  // now loop over configs
  for ( int emitter = 0; emitter < nreal; ++emitter ) {

    list<Ptr<SubtractionDipole>::ptr> matchDipoles;
    for ( vector<Ptr<SubtractionDipole>::ptr>::const_iterator d =
	    dipoles.begin(); d != dipoles.end(); ++d ) {
      if ( !(**d).canHandleEmitter(rep,emitter) )
	continue;
      matchDipoles.push_back(*d);
    }
    if ( matchDipoles.empty() )
      continue;

    for ( int emission = 2; emission < nreal; ++emission ) {
      if ( emission == emitter )
	continue;

      list<Ptr<SubtractionDipole>::ptr> matchDipoles2;
      for ( list<Ptr<SubtractionDipole>::ptr>::const_iterator d =
	      matchDipoles.begin(); d != matchDipoles.end(); ++d ) {
	if ( !(**d).canHandleSplitting(rep,emitter,emission) )
	  continue;
	matchDipoles2.push_back(*d);
      }
      if ( matchDipoles2.empty() )
	continue;

      map<Ptr<DiagramBase>::ptr,SubtractionDipole::MergeInfo> mergeInfo;

      for ( DiagramVector::const_iterator d = diagrams().begin(); d != diagrams().end(); ++d ) {

	Ptr<Tree2toNDiagram>::ptr check =
	  new_ptr(Tree2toNDiagram(*dynamic_ptr_cast<Ptr<Tree2toNDiagram>::ptr>(*d)));

	map<int,int> theMergeLegs;

	for ( unsigned int i = 0; i < check->external().size(); ++i )
	  theMergeLegs[i] = -1;
	int theEmitter = check->mergeEmission(emitter,emission,theMergeLegs);

	// no underlying Born
	if ( theEmitter == -1 )
	  continue;

	SubtractionDipole::MergeInfo info;
	info.diagram = check;
	info.emitter = theEmitter;
	info.mergeLegs = theMergeLegs;
	mergeInfo[*d] = info;

      }

      if ( mergeInfo.empty() )
	continue;

      for ( int spectator = 0; spectator < nreal; ++spectator ) {
	if ( spectator == emitter || spectator == emission )
	  continue;

	list<Ptr<SubtractionDipole>::ptr> matchDipoles3;
	for ( list<Ptr<SubtractionDipole>::ptr>::const_iterator d =
		matchDipoles2.begin(); d != matchDipoles2.end(); ++d ) {
	  if ( !(**d).canHandleSpectator(rep,spectator) )
	    continue;
	  matchDipoles3.push_back(*d);
	}
	if ( matchDipoles3.empty() )
	  continue;

	if ( noDipole(emitter,emission,spectator) )
	  continue;

	for ( list<Ptr<SubtractionDipole>::ptr>::const_iterator d =
		matchDipoles3.begin(); d != matchDipoles3.end(); ++d ) {

	  if ( !(**d).canHandle(rep,emitter,emission,spectator) )
	    continue;

	  for ( vector<Ptr<MatchboxMEBase>::ptr>::const_iterator b =
		  borns.begin(); b != borns.end(); ++b ) {
	    if ( (**b).onlyOneLoop() )
	      continue;
	    if ( done.find(make_pair(make_pair(make_pair(emitter,emission),spectator),make_pair(*b,*d))) 
		 != done.end() )
	      continue;
	    // now get to work
	    (**d).clearBookkeeping();
	    (**d).factory(factory());
	    (**d).realEmitter(emitter);
	    (**d).realEmission(emission);
	    (**d).realSpectator(spectator);
	    (**d).realEmissionME(const_cast<MatchboxMEBase*>(this));
	    (**d).underlyingBornME(*b);
	    (**d).setupBookkeeping(mergeInfo);
	    if ( !((**d).empty()) ) {
	      res.push_back((**d).cloneMe());
	      Ptr<SubtractionDipole>::tptr nDipole = res.back();
	      done.insert(make_pair(make_pair(make_pair(emitter,emission),spectator),make_pair(*b,*d)));
	      if ( nDipole->isSymmetric() )
		done.insert(make_pair(make_pair(make_pair(emission,emitter),spectator),make_pair(*b,*d)));
	      ostringstream dname;
	      dname << fullName() << "." << (**b).name() << "."
		    << (**d).name() << ".[(" 
		    << emitter << "," << emission << ")," << spectator << "]";
	      if ( ! (generator()->preinitRegister(nDipole,dname.str()) ) )
		throw Exception() << "MatchboxMEBase::getDipoles(): Dipole " << dname.str() << " already existing." << Exception::runerror;
	      if ( !factory()->reweighters().empty() ) {
		for ( vector<ReweightPtr>::const_iterator rw = factory()->reweighters().begin();
		      rw != factory()->reweighters().end(); ++rw )
		  nDipole->addReweighter(*rw);
	      }
	      if ( !factory()->preweighters().empty() ) {
		for ( vector<ReweightPtr>::const_iterator rw = factory()->preweighters().begin();
		      rw != factory()->preweighters().end(); ++rw )
		  nDipole->addPreweighter(*rw);
	      }
	      nDipole->cloneDependencies(dname.str());
	    }
	  }
	}
      }
    }
  }

  vector<Ptr<SubtractionDipole>::tptr> partners;
  copy(res.begin(),res.end(),back_inserter(partners));
  for ( vector<Ptr<SubtractionDipole>::ptr>::iterator d = res.begin();
	d != res.end(); ++d )
    (**d).partnerDipoles(partners);

  return res;

}

double MatchboxMEBase::colourCorrelatedME2(pair<int,int> ij) const {

  if ( matchboxAmplitude() ) {

    if ( matchboxAmplitude()->treeAmplitudes() )
      matchboxAmplitude()->prepareAmplitudes(this);

    double res = 
      matchboxAmplitude()->colourCorrelatedME2(ij)*
      me2Norm();

    return res;

  }

  throw Exception()
    << "MatchboxMEBase::colourCorrelatedME2() expects a MatchboxAmplitude object.\n"
    << "Please check your setup." << Exception::runerror;
  return 0.;

}

double MatchboxMEBase::largeNColourCorrelatedME2(pair<int,int> ij,
						 Ptr<ColourBasis>::tptr largeNBasis) const {

  if ( matchboxAmplitude() ) {

    if ( matchboxAmplitude()->treeAmplitudes() ) {
      largeNBasis->prepare(mePartonData(),false);
      matchboxAmplitude()->prepareAmplitudes(this);
    }

    double res = 
      matchboxAmplitude()->largeNColourCorrelatedME2(ij,largeNBasis)*
      me2Norm();

    return res;

  }

  throw Exception()
    << "MatchboxMEBase::largeNColourCorrelatedME2() expects a MatchboxAmplitude object.\n"
    << "Please check your setup." << Exception::runerror;
  return 0.;

}

double MatchboxMEBase::spinColourCorrelatedME2(pair<int,int> ij,
					       const SpinCorrelationTensor& c) const {

  if ( matchboxAmplitude() ) {

    if ( matchboxAmplitude()->treeAmplitudes() )
      matchboxAmplitude()->prepareAmplitudes(this);

    double res = 
      matchboxAmplitude()->spinColourCorrelatedME2(ij,c)*
      me2Norm();

    return res;

  }

  throw Exception()
    << "MatchboxMEBase::spinColourCorrelatedME2() expects a MatchboxAmplitude object.\n"
    << "Please check your setup." << Exception::runerror;
  return 0.;

}

double MatchboxMEBase::spinCorrelatedME2(pair<int,int> ij,
					 const SpinCorrelationTensor& c) const {

  if ( matchboxAmplitude() ) {

    if ( matchboxAmplitude()->treeAmplitudes() )
      matchboxAmplitude()->prepareAmplitudes(this);

    double res = 
      matchboxAmplitude()->spinCorrelatedME2(ij,c)*
      me2Norm();

    return res;

  }

  throw Exception()
    << "MatchboxMEBase::spinCorrelatedME2() expects a MatchboxAmplitude object.\n"
    << "Please check your setup." << Exception::runerror;
  return 0.;

}


void MatchboxMEBase::flushCaches() { 
  MEBase::flushCaches();
  if ( matchboxAmplitude() )
    matchboxAmplitude()->flushCaches();
  for ( vector<Ptr<MatchboxReweightBase>::ptr>::iterator r =
	  reweights().begin(); r != reweights().end(); ++r ) {
    (**r).flushCaches();
  }
  for ( vector<Ptr<MatchboxInsertionOperator>::ptr>::const_iterator v =
	  virtuals().begin(); v != virtuals().end(); ++v ) {
    (**v).flushCaches();
  }
}

void MatchboxMEBase::print(ostream& os) const {

  os << "--- MatchboxMEBase setup -------------------------------------------------------\n";

  os << " '" << name() << "' for subprocess:\n";

  os << "  ";
  for ( PDVector::const_iterator pp = subProcess().legs.begin();
	pp != subProcess().legs.end(); ++pp ) {
    os << (**pp).PDGName() << " ";
    if ( pp == subProcess().legs.begin() + 1 )
      os << "-> ";
  }
  os << "\n";

  os << " including " << (oneLoop() ? "" : "no ") << "virtual corrections";
  if ( oneLoopNoBorn() )
    os << " without Born contributions";
  if ( oneLoopNoLoops() )
    os << " without loop contributions";
  os << "\n";

  if ( oneLoop() && !onlyOneLoop() ) {
    os << " using insertion operators\n";
    for ( vector<Ptr<MatchboxInsertionOperator>::ptr>::const_iterator v =
	    virtuals().begin(); v != virtuals().end(); ++v ) {
      os << " '" << (**v).name() << "' with " 
	 << ((**v).isDR() ? "" : "C") << "DR/";
      if ( (**v).isCS() )
	os << "CS";
      if ( (**v).isBDK() )
	os << "BDK";
      if ( (**v).isExpanded() )
	os << "expanded";
      os << " conventions\n";
    }
  }

  os << "--------------------------------------------------------------------------------\n";

  os << flush;

}

void MatchboxMEBase::printLastEvent(ostream& os) const {

  os << "--- MatchboxMEBase last event information --------------------------------------\n";

  os << " for matrix element '" << name() << "'\n";

  os << " process considered:\n ";

  int in = 0;
  for ( cPDVector::const_iterator p = mePartonData().begin();
	p != mePartonData().end(); ++p ) {
    os << (**p).PDGName() << " ";
    if ( ++in == 2 )
      os << " -> ";
  }

  os << " kinematic environment as set by the XComb " << lastXCombPtr() << ":\n"
     << " sqrt(shat)/GeV = " << sqrt(lastSHat()/GeV2)
     << " x1 = " << lastX1() << " x2 = " << lastX2() 
     << " alphaS = " << lastAlphaS() << "\n";

  os << " momenta/GeV generated from random numbers\n ";
  copy(lastXComb().lastRandomNumbers().begin(),
       lastXComb().lastRandomNumbers().end(),ostream_iterator<double>(os," "));
  os << ":\n ";

  for ( vector<Lorentz5Momentum>::const_iterator p = meMomenta().begin();
	p != meMomenta().end(); ++p ) {
    os << (*p/GeV) << "\n ";
  }

  os << "last cross section/nb calculated was:\n "
     << (lastMECrossSection()/nanobarn) << " (pdf weight " << lastMEPDFWeight() << ")\n";

  os << "--------------------------------------------------------------------------------\n";

  os << flush;

}

void MatchboxMEBase::logGenerateKinematics(const double * r) const {

  if ( !verbose() )
    return;

  generator()->log() << "'" << name() << "' generated kinematics\nfrom "
		     << nDim() << " random numbers:\n";
  copy(r,r+nDim(),ostream_iterator<double>(generator()->log()," "));
  generator()->log() << "\n";

  generator()->log() << "storing phase space information in XComb "
		     << lastXCombPtr() << "\n";

  generator()->log() << "generated phase space point (in GeV):\n";

  vector<Lorentz5Momentum>::const_iterator pit = meMomenta().begin();
  cPDVector::const_iterator dit = mePartonData().begin();

  for ( ; pit != meMomenta().end() ; ++pit, ++dit )
    generator()->log() << (**dit).PDGName() << " : "
		       << (*pit/GeV) << "\n";

  generator()->log() << "with x1 = " << lastX1() << " x2 = " << lastX2() << "\n"
		     << "and Jacobian = " << jacobian() << " sHat/GeV2 = "
		     << (lastSHat()/GeV2) << "\n" << flush;

}

void MatchboxMEBase::logSetScale() const {

  if ( !verbose() )
    return;

  generator()->log() << "'" << name() << "' set scales using XComb " << lastXCombPtr() << ":\n"
		     << "scale/GeV2 = " << (scale()/GeV2) << " xi_R = "
		     << renormalizationScaleFactor() << " xi_F = "
		     << factorizationScaleFactor() << "\n"
		     << "alpha_s = " << lastAlphaS() << "\n" << flush;

}

void MatchboxMEBase::logPDFWeight() const {

  if ( !verbose() )
    return;

  generator()->log() << "'" << name() << "' calculated pdf weight = "
		     << lastMEPDFWeight() << " from XComb "
		     << lastXCombPtr() << "\n"
		     << "x1 = " << lastX1() << " (" << (mePartonData()[0]->coloured() ? "" : "not ") << "used) "
		     << "x2 = " << lastX2() << " (" << (mePartonData()[1]->coloured() ? "" : "not ") << "used)\n"
		     << flush;

}

void MatchboxMEBase::logME2() const {

  if ( !verbose() )
    return;

  generator()->log() << "'" << name() << "' evaluated me2 using XComb "
		     << lastXCombPtr() << "\n"
		     << "and phase space point (in GeV):\n";

  vector<Lorentz5Momentum>::const_iterator pit = meMomenta().begin();
  cPDVector::const_iterator dit = mePartonData().begin();

  for ( ; pit != meMomenta().end() ; ++pit, ++dit )
    generator()->log() << (**dit).PDGName() << " : "
		       << (*pit/GeV) << "\n";

  generator()->log() << "with x1 = " << lastX1() << " x2 = " << lastX2() << "\n"
		     << "sHat/GeV2 = " << (lastSHat()/GeV2) << "\n" << flush;

}

void MatchboxMEBase::logDSigHatDR() const {

  if ( !verbose() )
    return;

  generator()->log() << "'" << name() << "' evaluated cross section using XComb "
		     << lastXCombPtr() << "\n"
		     << "Jacobian = " << jacobian() << " sHat/GeV2 = "
		     << (lastSHat()/GeV2) << " dsig/nb = "
		     << (lastMECrossSection()/nanobarn) << "\n" << flush;

}

void MatchboxMEBase::cloneDependencies(const std::string& prefix) {

  if ( phasespace() ) {
    Ptr<MatchboxPhasespace>::ptr myPhasespace = phasespace()->cloneMe();
    ostringstream pname;
    pname << (prefix == "" ? fullName() : prefix) << "/" << myPhasespace->name();
    if ( ! (generator()->preinitRegister(myPhasespace,pname.str()) ) )
      throw Exception() << "MatchboxMEBase::cloneDependencies(): Phasespace generator " << pname.str() << " already existing." << Exception::runerror;
    myPhasespace->cloneDependencies(pname.str());
    phasespace(myPhasespace);
  }

  theAmplitude = dynamic_ptr_cast<Ptr<MatchboxAmplitude>::ptr>(amplitude());

  if ( matchboxAmplitude() ) {
    Ptr<MatchboxAmplitude>::ptr myAmplitude = matchboxAmplitude()->cloneMe();
    ostringstream pname;
    pname << (prefix == "" ? fullName() : prefix) << "/" << myAmplitude->name();
    if ( ! (generator()->preinitRegister(myAmplitude,pname.str()) ) )
      throw Exception() << "MatchboxMEBase::cloneDependencies(): Amplitude " << pname.str() << " already existing." << Exception::runerror;
    myAmplitude->cloneDependencies(pname.str());
    matchboxAmplitude(myAmplitude);
    amplitude(myAmplitude);
    matchboxAmplitude()->orderInGs(orderInAlphaS());
    matchboxAmplitude()->orderInGem(orderInAlphaEW());
  }

  if ( scaleChoice() ) {
    Ptr<MatchboxScaleChoice>::ptr myScaleChoice = scaleChoice()->cloneMe();
    ostringstream pname;
    pname << (prefix == "" ? fullName() : prefix) << "/" << myScaleChoice->name();
    if ( ! (generator()->preinitRegister(myScaleChoice,pname.str()) ) )
      throw Exception() << "MatchboxMEBase::cloneDependencies(): Scale choice " << pname.str() << " already existing." << Exception::runerror;
    scaleChoice(myScaleChoice);
  }

  for ( vector<Ptr<MatchboxReweightBase>::ptr>::iterator rw =
	  theReweights.begin(); rw != theReweights.end(); ++rw ) {
    Ptr<MatchboxReweightBase>::ptr myReweight = (**rw).cloneMe();
    ostringstream pname;
    pname << (prefix == "" ? fullName() : prefix) << "/" << (**rw).name();
    if ( ! (generator()->preinitRegister(myReweight,pname.str()) ) )
      throw Exception() << "MatchboxMEBase::cloneDependencies(): Reweight " << pname.str() << " already existing." << Exception::runerror;
    myReweight->cloneDependencies(pname.str());
    *rw = myReweight;
  }

  for ( vector<Ptr<MatchboxInsertionOperator>::ptr>::iterator v =
	  virtuals().begin(); v != virtuals().end(); ++v ) {
    Ptr<MatchboxInsertionOperator>::ptr myIOP = (**v).cloneMe();
    ostringstream pname;
    pname << (prefix == "" ? fullName() : prefix) << "/" << (**v).name();
    if ( ! (generator()->preinitRegister(myIOP,pname.str()) ) )
      throw Exception() << "MatchboxMEBase::cloneDependencies(): Insertion operator " << pname.str() << " already existing." << Exception::runerror;
    *v = myIOP;
  }

}

void MatchboxMEBase::prepareXComb(MatchboxXCombData& xc) const {

  // fixme We need to pass on the partons from the xcmob here, not
  // assuming one subprocess per matrix element

  if ( phasespace() )
    xc.nDimPhasespace(phasespace()->nDim(diagrams().front()->partons()));

  if ( matchboxAmplitude() ) {
    xc.nDimAmplitude(matchboxAmplitude()->nDimAdditional());
    if ( matchboxAmplitude()->colourBasis() ) {
      size_t cdim = 
 	matchboxAmplitude()->colourBasis()->prepare(diagrams(),noCorrelations());
      xc.colourBasisDim(cdim);
    }
    if ( matchboxAmplitude()->isExternal() ) {
      xc.externalId(matchboxAmplitude()->externalId(diagrams().front()->partons()));
    }
  }

  int insertionAdd = 0;
  for ( vector<Ptr<MatchboxInsertionOperator>::ptr>::const_iterator v =
	  virtuals().begin(); v != virtuals().end(); ++v ) {
    insertionAdd = max(insertionAdd,(**v).nDimAdditional());
  }
  xc.nDimInsertions(insertionAdd);

  xc.nLight(getNLight());

  for (size_t inlv=0; inlv<getNLightJetVec().size(); ++inlv)
    xc.nLightJetVec(getNLightJetVec()[inlv]);

  for (size_t inhv=0; inhv<getNHeavyJetVec().size(); ++inhv)
    xc.nHeavyJetVec(getNHeavyJetVec()[inhv]);

  for (size_t inlpv=0; inlpv<getNLightProtonVec().size(); ++inlpv)
    xc.nLightProtonVec(getNLightProtonVec()[inlpv]);

  xc.olpId(olpProcess());

  if ( initVerbose() ) {
    ostringstream fname_strm;
    // only allow alphanumeric, / and _ in filename
    BOOST_FOREACH (const char c, name()) {
        switch (c) {
          case '+' : fname_strm << "+"; break;
          case '-' : fname_strm << "-"; break;
          case '~' : fname_strm << "_tilde"; break;
          case ']' : break;
          case ',' : fname_strm << "__"; break;
          default  : fname_strm << (isalnum(c) ? c : '_'); break;
        }
    }
    fname_strm << ".diagrams";
    const string fname = fname_strm.str();
    ifstream test(fname.c_str());
    if ( !test ) {
      test.close();
      ofstream out(fname.c_str());
      for ( vector<Ptr<DiagramBase>::ptr>::const_iterator d = diagrams().begin();
	    d != diagrams().end(); ++d ) {
	DiagramDrawer::drawDiag(out,dynamic_cast<const Tree2toNDiagram&>(**d));
	out << "\n";
      }
    }
  }

}

StdXCombPtr MatchboxMEBase::makeXComb(Energy newMaxEnergy, const cPDPair & inc,
				      tEHPtr newEventHandler,tSubHdlPtr newSubProcessHandler,
				      tPExtrPtr newExtractor,	tCascHdlPtr newCKKW,
				      const PBPair & newPartonBins, tCutsPtr newCuts,
				      const DiagramVector & newDiagrams, bool mir,
				      const PartonPairVec&,
				      tStdXCombPtr newHead,
				      tMEPtr newME) {

  if ( !newME )
    newME = this;

  Ptr<MatchboxXComb>::ptr xc =
    new_ptr(MatchboxXComb(newMaxEnergy, inc,
			  newEventHandler, newSubProcessHandler,
			  newExtractor, newCKKW,
			  newPartonBins, newCuts, newME,
			  newDiagrams, mir,
			  newHead));

  prepareXComb(*xc);

  return xc;

}

StdXCombPtr MatchboxMEBase::makeXComb(tStdXCombPtr newHead,
				      const PBPair & newPartonBins,
				      const DiagramVector & newDiagrams,
				      tMEPtr newME) {
  if ( !newME )
    newME = this;

  Ptr<MatchboxXComb>::ptr xc = 
    new_ptr(MatchboxXComb(newHead, newPartonBins, newME, newDiagrams));

  prepareXComb(*xc);

  return xc;
}

void MatchboxMEBase::persistentOutput(PersistentOStream & os) const {
  os << theLastXComb << theFactory << thePhasespace 
     << theAmplitude << theScaleChoice << theVirtuals 
     << theReweights << theSubprocess << theOneLoop 
     << theOneLoopNoBorn << theOneLoopNoLoops
     << epsilonSquarePoleHistograms << epsilonPoleHistograms
     << theOLPProcess << theNoCorrelations
     << theHavePDFs << checkedPDFs<<theDiagramWeightVerboseDown<<theDiagramWeightVerboseUp;
}

void MatchboxMEBase::persistentInput(PersistentIStream & is, int) {
  is >> theLastXComb >> theFactory >> thePhasespace 
     >> theAmplitude >> theScaleChoice >> theVirtuals 
     >> theReweights >> theSubprocess >> theOneLoop 
     >> theOneLoopNoBorn >> theOneLoopNoLoops
     >> epsilonSquarePoleHistograms >> epsilonPoleHistograms
     >> theOLPProcess >> theNoCorrelations
     >> theHavePDFs >> checkedPDFs>>theDiagramWeightVerboseDown>>theDiagramWeightVerboseUp;
  lastMatchboxXComb(theLastXComb);
}

void MatchboxMEBase::Init() {

  static ClassDocumentation<MatchboxMEBase> documentation
    ("MatchboxMEBase is the base class for matrix elements "
     "in the context of the matchbox NLO interface.");

}

IBPtr MatchboxMEBase::clone() const {
  return new_ptr(*this);
}

IBPtr MatchboxMEBase::fullclone() const {
  return new_ptr(*this);
}

void MatchboxMEBase::doinit() {
  MEBase::doinit();
  if ( !theAmplitude )
    theAmplitude = dynamic_ptr_cast<Ptr<MatchboxAmplitude>::ptr>(amplitude());
  if ( matchboxAmplitude() )
    matchboxAmplitude()->init();
  if ( phasespace() ) {
    phasespace()->init();
    matchboxAmplitude()->checkReshuffling(phasespace());
  }
  if ( scaleChoice() ) {
    scaleChoice()->init();
  }
  for ( vector<Ptr<MatchboxReweightBase>::ptr>::iterator rw =
	  theReweights.begin(); rw != theReweights.end(); ++rw ) {
    (**rw).init();
  }
  for ( vector<Ptr<MatchboxInsertionOperator>::ptr>::iterator v =
	  virtuals().begin(); v != virtuals().end(); ++v ) {
    (**v).init();
  }
}



 void MatchboxMEBase::bookMEoverDiaWeight(double x) const {
   
      if (MEoverDiaWeight.size()==0){
	theDiagramWeightVerboseDown=min(theDiagramWeightVerboseDown,x*0.9);
	theDiagramWeightVerboseUp=max(theDiagramWeightVerboseUp,x*1.1);     
      }
      
      map<double,double>::iterator bx =MEoverDiaWeight.upper_bound(x);
      if ( bx == MEoverDiaWeight.end() ) {
	return;
      }
      bx->second += 1.;
      Nevents++;
      if (int(Nevents)%1000==0){
        ofstream out((RunDirectories::runStorage()+"/"+name()+"-MeoDiaW.dat").c_str());
	int i=0;
	double m=0.;
        for ( map<double,double>::const_iterator bx = MEoverDiaWeight.begin();bx != MEoverDiaWeight.end(); ++bx,i++ ) {
          out << " " << bx->first<<" "<<( bx->second/double(Nevents))<<"\n ";
	  m=max(m,bx->second/double(Nevents));
        }
        out.close();
	  ofstream gpout((RunDirectories::runStorage()+"/"+name()+"-MeoDiaW.gp").c_str());
  gpout << "set terminal epslatex color solid\n"
        << "set output '" << name()<<"-MeoDiaW"<< "-plot.tex'\n"
        << "#set logscale x\n"
        << "set xrange [" << theDiagramWeightVerboseDown << ":" << theDiagramWeightVerboseUp << "]\n"
        << "set yrange [0.:"<<(m*0.95)<<"]\n"
        << "set xlabel '$log(ME/\\sum DiaW)$'\n"
        << "set size 0.7,0.7\n"
        << "plot 1 w lines lc rgbcolor \"#DDDDDD\" notitle, '" << name()<<"-MeoDiaW"
        << ".dat' with histeps lc rgbcolor \"#00AACC\" t '$"<<name()<<"$'";
  
  gpout.close();
	

  
    }
  
}


void MatchboxMEBase::doinitrun() {
  MEBase::doinitrun();
  if ( matchboxAmplitude() )
    matchboxAmplitude()->initrun();
  if ( phasespace() ) {
    phasespace()->initrun();
  }
  if ( scaleChoice() ) {
    scaleChoice()->initrun();
  }
  for ( vector<Ptr<MatchboxReweightBase>::ptr>::iterator rw =
	  theReweights.begin(); rw != theReweights.end(); ++rw ) {
    (**rw).initrun();
  }
  for ( vector<Ptr<MatchboxInsertionOperator>::ptr>::iterator v =
	  virtuals().begin(); v != virtuals().end(); ++v ) {
    (**v).initrun();
  }

  if ( factory()->verboseDia() ) {
  
    for (  int k = 0; k < factory()->diagramWeightVerboseNBins() ; ++k ) {
      MEoverDiaWeight[theDiagramWeightVerboseDown+
		      double(k)*(theDiagramWeightVerboseUp-
				 theDiagramWeightVerboseDown)
		      /double(factory()->diagramWeightVerboseNBins()) ] = 0.;
	   
    }
    Nevents=0.;

    ofstream out("DiagramWeights.sh");
    out<<"P=$(pwd)"
       <<"\ncd "<<RunDirectories::runStorage()
       <<"\nrm -f DiagramWeights.tex"
       <<"\n echo \"\\documentclass{article}\" >> DiagramWeights.tex"
       <<"\n echo \"\\usepackage{amsmath,amsfonts,amssymb,graphicx,color}\" >> DiagramWeights.tex"
       <<"\n echo \"\\usepackage[left=2cm,right=2cm,top=2cm,bottom=2cm]{geometry}\" >> DiagramWeights.tex"
       <<"\n echo \"\\begin{document}\" >> DiagramWeights.tex"
       <<"\n echo \"\\setlength{\\parindent}{0cm}\" >> DiagramWeights.tex"
     
       <<"\n\n for i in $(ls *.gp | sed s/'\\.gp'//g) ; "
       <<"\n do"
       <<"\n     echo \"\\input{\"\"$i\"-plot\"}\" >> DiagramWeights.tex"
       <<"\n done"
       <<"\n  echo \"\\end{document}\" >> DiagramWeights.tex  "
       <<"\n  for i in *.gp ; do "
       <<"\n  gnuplot $i   "
       <<"\n   done      "
       <<"\n  pdflatex DiagramWeights.tex  \ncp DiagramWeights.pdf $P";
    out.close();

  }

}
  
void MatchboxMEBase::dofinish() {
  MEBase::dofinish();
  for ( map<cPDVector,AccuracyHistogram>::const_iterator
	  b = epsilonSquarePoleHistograms.begin();
	b != epsilonSquarePoleHistograms.end(); ++b ) {
    b->second.dump(factory()->poleData(),"epsilonSquarePoles-",b->first);
  }
  for ( map<cPDVector,AccuracyHistogram>::const_iterator
	  b = epsilonPoleHistograms.begin();
	b != epsilonPoleHistograms.end(); ++b ) {
    b->second.dump(factory()->poleData(),"epsilonPoles-",b->first);
  }
}

// *** Attention *** The following static variable is needed for the type
// description system in ThePEG. Please check that the template arguments
// are correct (the class and its base class), and that the constructor
// arguments are correct (the class name and the name of the dynamically
// loadable library where the class implementation can be found).
DescribeClass<MatchboxMEBase,MEBase>
describeHerwigMatchboxMEBase("Herwig::MatchboxMEBase", "Herwig.so");

