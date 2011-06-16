/*
 *  multi_bandwidth_alg.cc
 *  
 *
 *  Created by William March on 6/6/11.
 *  Copyright 2011 __MyCompanyName__. All rights reserved.
 *
 */

#include "multi_bandwidth_alg.h"


index_t npt::MultiBandwidthAlg::FindResultsInd_(
               const std::vector<index_t>& perm_locations) {
  
  index_t result = 0;
  index_t num_previous_bands = 1;
  
  for (index_t i = 0; i < perm_locations.size(); i++) {
   
    result += perm_locations[i] * num_previous_bands;
    num_previous_bands *= num_bands_[i];
    
  }
  
} // FindResultsInd

// this is the inverse of the function above
void npt::MultiBandwidthAlg::FindMatcherInd_(index_t loc,
                                             std::vector<index_t>& result) {
  
  //std::vector<index_t> result(num_bands_.size());

  index_t new_loc = loc;

  index_t mod_fac = 1;
  
  for (index_t i = 0; i < num_bands_.size(); i++) {
    mod_fac *= num_bands_[i];
  }
  
  
  for (int i = num_bands_.size() - 1; i >= 0; i--) {
    
    result[i] = new_loc;
    
    mod_fac = mod_fac / num_bands_[i];
    
    new_loc = new_loc % mod_fac;
    
  } // for i
  

  mod_fac = num_bands_[0];
  for (index_t i = 1; i < result.size(); i++) {
    
    for (index_t j = 0; j < i; j++) {
    
      result[i] = result[i] - result[j];
    
    } // for j
    
    result[i] = result[i] / mod_fac;
    
    mod_fac = mod_fac * num_bands_[i];
    
  } // for i
  
} // FindMatcherInd


void npt::MultiBandwidthAlg::BaseCaseHelper_(
                         std::vector<std::vector<index_t> >& point_sets,
                         std::vector<bool>& permutation_ok,
                         std::vector<std::vector<index_t> >& perm_locations,
                         std::vector<index_t>& points_in_tuple,
                         int k) {
  
  
  // perm_locations[i][j] = k means that in the ith permutation, that 
  // matcher_dists_[j][k] is the current entry in the matcher that this tuple
  // satisfies
  
  std::vector<bool> perm_ok_copy(permutation_ok);
  std::vector<std::vector<index_t> > perm_locations_copy(perm_locations);

  bool bad_symmetry = false;
  
  // iterate over possible new points
  for (index_t i = 0; i < point_sets[k].size(); i++) {
    
    index_t new_point_ind = point_sets[k][i];
    bool this_point_works = true;
    
    bad_symmetry = false;
    
    arma::colvec new_point_vec = data_points_.col(new_point_ind);
   
    // copy the permutation 
    perm_ok_copy.assign(permutation_ok.begin(), permutation_ok.end());
    
    // TODO: check if I can accurately copy this more directly
    for (index_t m = 0; m < perm_locations_copy.size(); m++) {
      perm_locations_copy[m].assign(perm_locations[m].begin(), 
                                    perm_locations[m].end());
    } // for m
    
    // TODO: double check that I can exit on bad symmetry here
    for (index_t j = 0; j < k && this_point_works && !bad_symmetry; j++) {
      
      index_t old_point_ind = points_in_tuple[j];
      
      bad_symmetry = (new_point_ind <= old_point_ind);
      
      if (!bad_symmetry) {
        
        arma::colvec old_point_vec = data_points_.col(old_point_ind);
        
        double point_dist_sq = la::DistanceSqEuclidean(old_point_vec, 
                                                       new_point_vec);
        
        this_point_works = matcher_.TestPointPair(point_dist_sq, j, k, 
                                                  perm_ok_copy,
                                                  perm_locations_copy);
        
      } // check symmetry
      
    } // check existing points
    
    if (this_point_works && !bad_symmetry) {
      
      points_in_tuple[k] = new_point_ind;
      
      if (k == tuple_size_ - 1) {
        
        // fill in all the results that worked
        
        for (index_t n = 0; n < perm_locations_copy.size(); n++) {
          
          if (perm_ok_copy[n]) {
            index_t results_ind = FindResultsInd_(perm_locations_copy[n]);
            results_[results_ind]++;
          }
        } // for n
        
        
      }
      else {
        
        BaseCaseHelper_(point_sets, perm_ok_copy, perm_locations_copy,
                        points_in_tuple, k+1);
        
      }
      
    }
    
  } // iterate over possible new points
  
  
} // BaseCaseHelper_

void npt::MultiBandwidthAlg::BaseCase_(NodeTuple& nodes) {
  
  std::vector<std::vector<index_t> > point_sets(tuple_size_);
  
  for (index_t node_ind = 0; node_ind < tuple_size_; node_ind++) {
    
    point_sets[node_ind].resize(nodes.node_list(node_ind)->count());
    
    for (index_t i = 0; i < nodes.node_list(node_ind)->count(); i++) {
      
      point_sets[node_ind][i] = i + nodes.node_list(node_ind)->begin();
      
    } // for i
    
  } // for node_ind
  
  std::vector<bool> permutation_ok(matcher_.num_permutations(), true);
  
  std::vector<index_t> points_in_tuple(tuple_size_, -1);
  
  std::vector<std::vector<index_t> > perm_locations(num_permutations_);

  for (index_t i = 0; i < perm_locations.size(); i++) {
    perm_locations[i].resize(num_bands_.size(), INT_MAX);
  }
  
  BaseCaseHelper_(point_sets, permutation_ok, perm_locations, 
                  points_in_tuple, 0);
  
  
} // BaseCase_




bool npt::MultiBandwidthAlg::CanPrune_(NodeTuple& nodes) {

  return (!(matcher_.TestNodeTuple(nodes)));
  
} // CanPrune_




void npt::MultiBandwidthAlg::DepthFirstRecursion_(NodeTuple& nodes) {
  
  if (CanPrune_(nodes)) {
    num_prunes_++;
  }
  else if (nodes.all_leaves()) {
    
    BaseCase_(nodes);
    
  } // base case
  else {
    
    if (nodes.CheckSymmetry(nodes.ind_to_split(), true)) {
      
      NodeTuple left_child(nodes, true);
      DepthFirstRecursion_(left_child);
      
    }
    
    if (nodes.CheckSymmetry(nodes.ind_to_split(), false)) {

      NodeTuple right_child(nodes, false);
      DepthFirstRecursion_(right_child);
      
    }
    
  } // recursing
  
} // Recursion
          
        
void npt::MultiBandwidthAlg::OutputResults() {
  
  //std::cout << "First result: " << results_[0] << "\n\n";
  
  for (index_t i = 0; i < results_.size(); i++) {
    
    std::vector<index_t> matcher_ind(num_bands_.size());
    FindMatcherInd_(i, matcher_ind);
    
    std::cout << "Matcher: ";
    for (index_t j = 0; j < matcher_ind.size(); j++) {
      
      std::cout << matcher_.matcher_dists(j, matcher_ind[j]) << ", ";
      
    }
    std::cout << ": " << results_[i] << "\n";
    
  } // for i
  
  
} // OutputResults
