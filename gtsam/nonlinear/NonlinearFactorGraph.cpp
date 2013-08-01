/* ----------------------------------------------------------------------------

 * GTSAM Copyright 2010, Georgia Tech Research Corporation, 
 * Atlanta, Georgia 30332-0415
 * All Rights Reserved
 * Authors: Frank Dellaert, et al. (see THANKS for the full author list)

 * See LICENSE for the license information

 * -------------------------------------------------------------------------- */

/**
 * @file    NonlinearFactorGraph.cpp
 * @brief   Factor Graph Consisting of non-linear factors
 * @author  Frank Dellaert
 * @author  Carlos Nieto
 * @author  Christian Potthast
 */

#include <cmath>
#include <limits>
#include <boost/foreach.hpp>
#include <gtsam/inference/FactorGraph-inst.h>
#include <gtsam/geometry/Pose2.h>
#include <gtsam/geometry/Pose3.h>
#include <gtsam/linear/GaussianFactorGraph.h>
#include <gtsam/nonlinear/Values.h>
#include <gtsam/inference/Ordering.h>
#include <gtsam/nonlinear/NonlinearFactorGraph.h>

using namespace std;

namespace gtsam {

/* ************************************************************************* */
double NonlinearFactorGraph::probPrime(const Values& c) const {
  return exp(-0.5 * error(c));
}

/* ************************************************************************* */
void NonlinearFactorGraph::print(const std::string& str, const KeyFormatter& keyFormatter) const {
  cout << str << "size: " << size() << endl;
  for (size_t i = 0; i < factors_.size(); i++) {
    stringstream ss;
    ss << "factor " << i << ": ";
    if (factors_[i] != NULL) factors_[i]->print(ss.str(), keyFormatter);
  }
}

/* ************************************************************************* */
void NonlinearFactorGraph::saveGraph(std::ostream &stm, const Values& values,
    const GraphvizFormatting& graphvizFormatting,
    const KeyFormatter& keyFormatter) const
{
  stm << "graph {\n";
  stm << "  size=\"" << graphvizFormatting.figureWidthInches << "," <<
    graphvizFormatting.figureHeightInches << "\";\n\n";

  FastSet<Key> keys = this->keys();

  // Local utility function to extract x and y coordinates
  struct { boost::optional<Point2> operator()(
    const Value& value, const GraphvizFormatting& graphvizFormatting)
  {
    if(const Pose2* p = dynamic_cast<const Pose2*>(&value)) {
      double x, y;
      switch (graphvizFormatting.paperHorizontalAxis) {
      case GraphvizFormatting::X: x = p->x(); break;
      case GraphvizFormatting::Y: x = p->y(); break;
      case GraphvizFormatting::Z: x = 0.0; break;
      case GraphvizFormatting::NEGX: x = -p->x(); break;
      case GraphvizFormatting::NEGY: x = -p->y(); break;
      case GraphvizFormatting::NEGZ: x = 0.0; break;
      default: throw std::runtime_error("Invalid enum value");
      }
      switch (graphvizFormatting.paperVerticalAxis) {
      case GraphvizFormatting::X: y = p->x(); break;
      case GraphvizFormatting::Y: y = p->y(); break;
      case GraphvizFormatting::Z: y = 0.0; break;
      case GraphvizFormatting::NEGX: y = -p->x(); break;
      case GraphvizFormatting::NEGY: y = -p->y(); break;
      case GraphvizFormatting::NEGZ: y = 0.0; break;
      default: throw std::runtime_error("Invalid enum value");
      }
      return Point2(x,y);
    } else if(const Pose3* p = dynamic_cast<const Pose3*>(&value)) {
      double x, y;
      switch (graphvizFormatting.paperHorizontalAxis) {
      case GraphvizFormatting::X: x = p->x(); break;
      case GraphvizFormatting::Y: x = p->y(); break;
      case GraphvizFormatting::Z: x = p->z(); break;
      case GraphvizFormatting::NEGX: x = -p->x(); break;
      case GraphvizFormatting::NEGY: x = -p->y(); break;
      case GraphvizFormatting::NEGZ: x = -p->z(); break;
      default: throw std::runtime_error("Invalid enum value");
      }
      switch (graphvizFormatting.paperVerticalAxis) {
      case GraphvizFormatting::X: y = p->x(); break;
      case GraphvizFormatting::Y: y = p->y(); break;
      case GraphvizFormatting::Z: y = p->z(); break;
      case GraphvizFormatting::NEGX: y = -p->x(); break;
      case GraphvizFormatting::NEGY: y = -p->y(); break;
      case GraphvizFormatting::NEGZ: y = -p->z(); break;
      default: throw std::runtime_error("Invalid enum value");
      }
     return Point2(x,y);
    } else {
      return boost::none;
    }
  }} getXY;

  // Find bounds
  double minX = numeric_limits<double>::infinity(), maxX = -numeric_limits<double>::infinity();
  double minY = numeric_limits<double>::infinity(), maxY = -numeric_limits<double>::infinity();
  BOOST_FOREACH(Key key, keys) {
    if(values.exists(key)) {
      boost::optional<Point2> xy = getXY(values.at(key), graphvizFormatting);
      if(xy) {
        if(xy->x() < minX)
          minX = xy->x();
        if(xy->x() > maxX)
          maxX = xy->x();
        if(xy->y() < minY)
          minY = xy->y();
        if(xy->y() > maxY)
          maxY = xy->y();
      }
    }
  }

  // Create nodes for each variable in the graph
  BOOST_FOREACH(Key key, keys) {
    // Label the node with the label from the KeyFormatter
    stm << "  var" << key << "[label=\"" << keyFormatter(key) << "\"";
    if(values.exists(key)) {
      boost::optional<Point2> xy = getXY(values.at(key), graphvizFormatting);
      if(xy)
        stm << ", pos=\"" << graphvizFormatting.scale*(xy->x() - minX) << "," << graphvizFormatting.scale*(xy->y() - minY) << "!\"";
    }
    stm << "];\n";
  }
  stm << "\n";

  if(graphvizFormatting.mergeSimilarFactors) {
    // Remove duplicate factors
    FastSet<vector<Key> > structure;
    BOOST_FOREACH(const sharedFactor& factor, *this) {
      if(factor) {
        vector<Key> factorKeys = factor->keys();
        std::sort(factorKeys.begin(), factorKeys.end());
        structure.insert(factorKeys);
      }
    }

    // Create factors and variable connections
    size_t i = 0;
    BOOST_FOREACH(const vector<Key>& factorKeys, structure) {
      // Make each factor a dot
      stm << "  factor" << i << "[label=\"\", shape=point";
      {
        map<size_t, Point2>::const_iterator pos = graphvizFormatting.factorPositions.find(i);
        if(pos != graphvizFormatting.factorPositions.end())
          stm << ", pos=\"" << graphvizFormatting.scale*(pos->second.x() - minX) << "," << graphvizFormatting.scale*(pos->second.y() - minY) << "!\"";
      }
      stm << "];\n";

      // Make factor-variable connections
      BOOST_FOREACH(Key key, factorKeys) {
        stm << "  var" << key << "--" << "factor" << i << ";\n"; }

      ++ i;
    }
  } else {
    // Create factors and variable connections
    for(size_t i = 0; i < this->size(); ++i) {
      // Make each factor a dot
      stm << "  factor" << i << "[label=\"\", shape=point];\n";

      // Make factor-variable connections
      if(this->at(i)) {
        BOOST_FOREACH(Key key, *this->at(i)) {
          stm << "  var" << key << "--" << "factor" << i << ";\n"; } }
    }
  }

  stm << "}\n";
}

/* ************************************************************************* */
double NonlinearFactorGraph::error(const Values& c) const {
  gttic(NonlinearFactorGraph_error);
  double total_error = 0.;
  // iterate over all the factors_ to accumulate the log probabilities
  BOOST_FOREACH(const sharedFactor& factor, this->factors_) {
    if(factor)
      total_error += factor->error(c);
  }
  return total_error;
}

/* ************************************************************************* */
FastSet<Key> NonlinearFactorGraph::keys() const {
  FastSet<Key> keys;
  BOOST_FOREACH(const sharedFactor& factor, this->factors_) {
    if(factor)
      keys.insert(factor->begin(), factor->end());
  }
  return keys;
}

/* ************************************************************************* */
Ordering NonlinearFactorGraph::orderingCOLAMD() const
{
  return Ordering::COLAMD(*this);
}

/* ************************************************************************* */
Ordering NonlinearFactorGraph::orderingCOLAMDConstrained(const FastMap<Key, int>& constraints) const
{
  return Ordering::COLAMDConstrained(*this, constraints);
}

/* ************************************************************************* */
//SymbolicFactorGraphOrdered::shared_ptr NonlinearFactorGraph::symbolic(const OrderingOrdered& ordering) const {
//  gttic(NonlinearFactorGraph_symbolic_from_Ordering);
//
//  // Generate the symbolic factor graph
//  SymbolicFactorGraphOrdered::shared_ptr symbolicfg(new SymbolicFactorGraphOrdered);
//  symbolicfg->reserve(this->size());
//
//  BOOST_FOREACH(const sharedFactor& factor, this->factors_) {
//    if(factor)
//      symbolicfg->push_back(factor->symbolic(ordering));
//    else
//      symbolicfg->push_back(SymbolicFactorGraphOrdered::sharedFactor());
//  }
//
//  return symbolicfg;
//}

/* ************************************************************************* */
//pair<SymbolicFactorGraphOrdered::shared_ptr, OrderingOrdered::shared_ptr> NonlinearFactorGraph::symbolic(
//    const Values& config) const
//{
//  gttic(NonlinearFactorGraph_symbolic_from_Values);
//
//  // Generate an initial key ordering in iterator order
//  OrderingOrdered::shared_ptr ordering(config.orderingArbitrary());
//  return make_pair(symbolic(*ordering), ordering);
//}

/* ************************************************************************* */
GaussianFactorGraph::shared_ptr NonlinearFactorGraph::linearize(const Values& linearizationPoint) const
{
  gttic(NonlinearFactorGraph_linearize);

  // create an empty linear FG
  GaussianFactorGraph::shared_ptr linearFG = boost::make_shared<GaussianFactorGraph>();
  linearFG->reserve(this->size());

  // linearize all factors
  BOOST_FOREACH(const sharedFactor& factor, this->factors_) {
    if(factor) {
      (*linearFG) += factor->linearize(linearizationPoint);
    } else
      (*linearFG) += GaussianFactor::shared_ptr();
  }

  return linearFG;
}

/* ************************************************************************* */
NonlinearFactorGraph NonlinearFactorGraph::clone() const {
  NonlinearFactorGraph result;
  BOOST_FOREACH(const sharedFactor& f, *this) {
    if (f)
      result.push_back(f->clone());
    else
      result.push_back(sharedFactor()); // Passes on null factors so indices remain valid
  }
  return result;
}

/* ************************************************************************* */
NonlinearFactorGraph NonlinearFactorGraph::rekey(const std::map<Key,Key>& rekey_mapping) const {
  NonlinearFactorGraph result;
  BOOST_FOREACH(const sharedFactor& f, *this) {
    if (f)
      result.push_back(f->rekey(rekey_mapping));
    else
      result.push_back(sharedFactor());
  }
  return result;
}

/* ************************************************************************* */

} // namespace gtsam
