/**
 * \file GraphED.hh
 * \author Martial Bourquin
 * \version 1.0
 * \date 30/06/2012
 */

#ifndef GRAPHED_HH_
# define GRAPHED_HH_

# include <stdlib.h>
# include "BinSlay.hpp"
# include "Node.hh"
# include "AGraph.hpp"
# include "Hungarian.hh"

namespace BinSlay
{
  typedef unsigned int GED;

  template<typename NodeType>
  class GraphED
  {
  public:
    GraphED(
	    AGraph<NodeType> const &g1,
	    AGraph<NodeType> const &g2,
	    typename BinSlay::bind_node<NodeType>::NODES_LIST &setA,
	    typename BinSlay::bind_node<NodeType>::NODES_LIST &setB)
      : _g1(g1),
	_g2(g2),
	_ged(-1),
	_osize_g1(setA.size()),
	_osize_g2(setB.size()),
	_cost_matrix_size(0),
	_matrix(nullptr),
	_edit_path(nullptr)
    {
      //
      // STEP0: Extend the number of nodes in the two graphs if their sizes are not equal.
      // 
      if (setA.size() != setB.size()) {
	unsigned nb_dummy_nodes_to_add_in_setA = setB.size();
	unsigned nb_dummy_nodes_to_add_in_setB = setA.size();

	// A 'null' element is a dummy node
	while (nb_dummy_nodes_to_add_in_setA--)
	  setA.push_back(nullptr);
	while (nb_dummy_nodes_to_add_in_setB--)
	  setB.push_back(nullptr);
	//
	// STEP1: Compute the size of the cost matrix which is a square matrix and initialise it.
	//
	// Let 's_g1' be the size of the graph 'g1' which nodes are in the 'setA' and 's_g2'
	// the size of the graph 'g2' which nodes are in the 'setB'. The resulting cost matrix
	// would be of size: (s_g1 + s_g2) * (s_g2 + s_g1).
	//      
	this->_cost_matrix_size = (this->_osize_g1 + this->_osize_g2);
      } else {
	// Sizes are equal: this means no insertion and no deletion are needed
	this->_cost_matrix_size = this->_osize_g1;
      }
     
      // TODO: Need some explanation!
      // We need to keep trace of original
      // Fill '_cols' vector with nodes from list setB to cache data.
      this->_rows.resize(setA.size());
      this->_cols.resize(setB.size());
      {
	unsigned int col = 0;
	for (auto it = setB.begin(); it != setB.end(); ++it)
	  this->_cols[col++] = *it;
      }
      
      // TODO: WTF ????? Need to be fixed!
      // Allocate the Cost matrix
      this->_matrix = reinterpret_cast<BinSlay::Hungarian::Cost*>(
		malloc(sizeof(*this->_matrix) * this->_cost_matrix_size * this->_cost_matrix_size));
      if (!this->_matrix) {
	// TODO: run-time exception
	std::cout << "MALLOC FAILED!" << std::endl;
	exit(1);
      }

      //
      // STEP2: Fill the cost matrix.
      //
      // In the upper left corner is a submatrix of size s_g1 * s_g2 which represents the cost of node
      // substitution (e.g. the relabeling cost).
      //
      // In the upper right corner is a submatrix of size s_g1 * s_g2 which represents the cost of node
      // deletion.
      //
      // In the bottom right is a submatrix of size s_g1 * s_g2 which represents the cost of node
      // instertion.
      //
      // In the bottom right is a zero submatrix of size s_g1 * s_g2 which represents a zero-cost
      // substitution (e.g. associated a dummy node with another dummy node).
      //
      // Note that the diagonal of the right upper corner represents the cost of all possible nodes
      // deletions, the diagonal of the bottom left corner the cost of all possible node insertations
      //
      unsigned int row = 0;  
      for (auto itA = setA.begin(); itA != setA.end(); ++itA) {
	// Fill '_rows' vector with nodes from list setA to cache data
	this->_rows[row] = *itA;

	unsigned int col = 0;      
	for (auto itB = setB.begin(); itB != setB.end(); ++itB) {
	  if (*itA && *itB) {
	    // upper left: substitution
	    this->_matrix[row * _cost_matrix_size + col]._cost =
	      (*itA)->assign_substitution_cost(**itB);
	  } else if (*itA && !*itB) {
	    // upper right: deletion in g1
	    if (col - this->_osize_g2 == row) 
	      this->_matrix[row * _cost_matrix_size + col]._cost =
		(*itA)->assign_deletion_cost();
	    else
	      this->_matrix[row * _cost_matrix_size + col]._cost = 0xafffffff;
	  } else if (!*itA && *itB) {
	    // bottom left: insertion in g1
	    if (row - this->_osize_g1 == col)
	      this->_matrix[row * _cost_matrix_size + col]._cost =
		(*itB)->assign_insertion_cost();
	    else
	      this->_matrix[row * _cost_matrix_size + col]._cost = 0xafffffff;
	  } else {
	    // bottom right: dummy with dummy => 0 cost
	    this->_matrix[row * _cost_matrix_size + col]._cost = 0;
	  }

	  // Init the booleans to false
	  this->_matrix[row * _cost_matrix_size + col]._starred = false;
	  this->_matrix[row * _cost_matrix_size + col]._primed = false;
	  this->_matrix[row * _cost_matrix_size + col]._series = false;
    
	  // Set the '_save_cost' filed
	  this->_matrix[row * _cost_matrix_size + col]._saved_cost =
	    this->_matrix[row * _cost_matrix_size + col]._cost;

	  ++col;
	}
	++row;
      }
    }

    void update_cost(unsigned int cost, unsigned int row, unsigned int col)
    {
      this->_matrix[row * _cost_matrix_size + col]._cost += cost;
    }

    void setEditPath(typename BinSlay::bind_node<NodeType>::ISOMORPHES_LIST *ep)
    {
      this->_edit_path = ep;
    }
    
    //    GraphED(GraphED const &);
    ~GraphED() {}
    //    GraphED &operator=(GraphED const &);
    
    GED compute()
    {
      // Solve the assignment problem with the Hungarian's algorithm
      if (_osize_g1 <= _osize_g2)
	BinSlay::Hungarian(this->_matrix, this->_cost_matrix_size, true).solve();
      else
	BinSlay::Hungarian(this->_matrix, this->_cost_matrix_size, false).solve();

      // Compute the approximation of the GED by collecting the results from the
      // Hungarian algorithm
      this->_ged = 0;
      for (size_t row = 0; row < this->_cost_matrix_size; ++row)
	for (size_t col = 0; col < this->_cost_matrix_size; ++col)
	  if (this->_matrix[row * _cost_matrix_size + col]._starred) {
	    this->_ged += this->_matrix[row * _cost_matrix_size + col]._saved_cost;
	  }

      // Create the edit_path, a list of Isomorphe objects
      auto *ret = new typename BinSlay::bind_node<NodeType>::ISOMORPHES_LIST;

      // Fill the edit path and compute the number of substitutions, insertions and
      // deletions for debug and experiment results
      unsigned int nb_substitutions = 0;
      unsigned int nb_insertions = 0;
      unsigned int nb_deletions = 0;
      for (size_t row = 0; row < this->_cost_matrix_size; ++row)
	for (size_t col = 0; col < this->_cost_matrix_size; ++col)
	  if (this->_matrix[row * _cost_matrix_size + col]._starred) {

	    if (!(!_rows[row] && !_cols[col])) {
	      ret->push_front(new BinSlay::Isomorphes<NodeType>(
				_rows[row],
				_cols[col],
				this->_matrix[row * _cost_matrix_size + col]._saved_cost)
	      );
	      if (_rows[row] && _cols[col])
		++nb_substitutions;
	      else if (!_rows[row] && _cols[col])
		++nb_insertions;
	      if (_rows[row] && !_cols[col])
		++nb_deletions;
	    }
	  }
#ifdef BINSLAYER_DEBUG
      std::cerr << std::dec << "swap: " << nb_substitutions
		<< " - add: " << nb_insertions
		<< " - del:" << nb_deletions
		<< std::endl;
#endif // !BINSLAYER_DEBUG
      this->_edit_path = ret;
      return this->_ged;
    }
    
    GED getGED()
    {
      // Check if you have already computed the GED and if not, compute it
      if (this->_ged == 0xffffffff)
	this->compute();
      return this->_ged;
    }

    typename BinSlay::bind_node<NodeType>::ISOMORPHES_LIST *get_edit_path() const
    {
      return this->_edit_path;
    }

  private:
    AGraph<NodeType> const &_g1;
    AGraph<NodeType> const &_g2;
    GED	_ged;
    unsigned int _osize_g1;
    unsigned int _osize_g2;
    unsigned int _cost_matrix_size;
    BinSlay::Hungarian::Cost *_matrix;
    typename BinSlay::bind_node<NodeType>::ISOMORPHES_LIST *_edit_path;

    // Use to ease the mapping of nodes between the two sets at
    // the end of the algorithm: create the IsoList.
    std::vector<NodeType *> _rows;
    std::vector<NodeType *> _cols;
  };
}

#endif // !GRAPHED_HH_
