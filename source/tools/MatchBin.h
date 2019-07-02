/**
 *  @note This file is part of Empirical, https://github.com/devosoft/Empirical
 *  @copyright Copyright (C) Michigan State University, MIT Software license; see doc/LICENSE.md
 *  @date 2019
 *
 *  @file MatchBin.h
 *  @brief A container that supports flexible tag-based lookup. .
 *
 */


#ifndef EMP_MATCH_BIN_H
#define EMP_MATCH_BIN_H

#include <iostream>
#include <unordered_map>
#include <unordered_set>
#include <functional>
#include <algorithm>
#include <stdexcept>
#include <limits>
#include <ratio>
#include <math.h>

#include "../base/assert.h"
#include "../base/vector.h"
#include "../tools/IndexMap.h"
#include "../tools/BitSet.h"

namespace emp {

  /// Metric for MatchBin stored in the struct so we can template on it
  /// Returns the number of bits not in common between two BitSets
  template<size_t Width>
  struct HammingMetric {

    using tag_t = emp::BitSet<Width>;
    using query_t = emp::BitSet<Width>;

    double operator()(const query_t& a, const tag_t& b) const{
      return (double)(a^b).CountOnes();
    }
  };

  /// Metric gives the absolute difference between two integers
  struct AbsDiffMetric {

    using tag_t = int;
    using query_t = int;

    double operator()(const query_t& a, const tag_t& b) const {
      return (double)std::abs(a-b);
    }
  };

  /// Metric gives the matchings by the closest tag on or above itself.
  /// Wraps on Max.
  /// Adapted from Spector, Lee, et al. "Tag-based modules in genetic programming." Proceedings of the 13th annual conference on Genetic and evolutionary computation. ACM, 2011.
  template<size_t Max=1000>
  struct NextUpMetric {

    using tag_t = size_t;
    using query_t = size_t;

    double operator()(const query_t& a, const tag_t& b) const {
      const size_t difference = ((Max + 1) + b - a) % (Max + 1);
      return (double)(difference % (Max + 1));
    }
  };

  /// Matches based on the longest segment of equal and uneqal bits in two bitsets
  /// Adapted from Downing, Keith L. Intelligence emerging: adaptivity and search in evolving neural systems. MIT Press, 2015.
  template<size_t Width>
  struct StreakMetric {

    using tag_t = emp::BitSet<Width>;
    using query_t = emp::BitSet<Width>;

    double operator()(const query_t& a, const tag_t& b) const {
      const emp::BitSet<Width> bs = a^b;
      const size_t same = (~bs).LongestSegmentOnes();
      const size_t different = bs.LongestSegmentOnes();
      const double ps = ProbabilityKBitSequence(same);
      const double pd = ProbabilityKBitSequence(different);

      const double match = (pd / (ps + pd));
      // Note: here, close match score > poor match score
      // However, we're computing distance where smaller means closer match.
      // Note also: 0.0 < match < 1.0
      // So, we subtract match score from 1.0 to get a distance.
      return 1.0 - match;
    }

    inline double ProbabilityKBitSequence(size_t k) const {
      return (Width - k + 1) / std::pow(2, k);
    }
  };

  /// Metric gives the absolute value of the difference between the integer
  /// representations of the BitSets.
  /// Adapted from Downing, Keith L. Intelligence emerging: adaptivity and search in evolving neural systems. MIT Press, 2015.
  template<size_t Width>
  struct AbsIntDiffMetric {

    using tag_t = emp::BitSet<Width>;
    using query_t = emp::BitSet<Width>;

    double operator()(const query_t& a, const tag_t& b) {
      emp::BitSet<Width> bitDifference = ( a > b ? a - b : b - a);
      static_assert(Width <= 32);
      return bitDifference.GetUInt(0);
    }
  };


  /// Abstract base class for selectors
  struct Selector {

    virtual ~Selector() {};
    virtual emp::vector<size_t> operator()(
      emp::vector<size_t>& uids,
      std::unordered_map<size_t, double>& scores,
      size_t n
    ) = 0;
  };

  /// Returns matches within the threshold ThreshRatio sorted by match quality.
  template<typename ThreshRatio = std::ratio<-1,1>> // neg numerator means +infy
  struct RankedSelector : public Selector {
    emp::vector<size_t> operator()(
      emp::vector<size_t>& uids,
      std::unordered_map<size_t, double>& scores,
      size_t n
    ){

      size_t back = 0;

      // treat any negative numerator as positive infinity
      const double thresh = (
        ThreshRatio::num < 0
        ? std::numeric_limits<double>::infinity()
        : ((double) ThreshRatio::num) / ThreshRatio::den
      );

      if (n < std::log2(uids.size())) {

        // Perform a bounded selection sort to find the first n results
        for (; back < n; ++back) {
          int minIndex = -1;
          for (size_t j = back; j < uids.size(); ++j) {
            if (
              (minIndex == -1 || scores.at(uids[j]) < scores.at(uids[minIndex]))
              && (scores.at(uids[j]) <= thresh)
            ) {
              minIndex = j;
            }
          }
          if (minIndex == -1) break;
          std::swap(uids.at(back),uids.at(minIndex));
        }

      } else {

        std::sort(
          uids.begin(),
          uids.end(),
          [&scores](const size_t &a, const size_t &b) {
            return scores.at(a) < scores.at(b);
          }
        );

        while (
          back < uids.size()
          && back < n
          && scores.at(uids[back]) <= thresh
        ) ++back;

      }

      return emp::vector<size_t>(uids.begin(), uids.begin() + back);
    }
  };

  /// Selector chooses probabilistically based on match quality with replacement.
  /// ThreshRatio: what is the raw maximum score to even be considered to match
  /// SkewRatio: how much more heavily should the best matches be weighted
  /// in terms of match probability; must be greater than 0
  /// (close to zero: very heavily, large: mostly even weighting)
  /// MaxBaselineRatio: maximum score that all scores will be normalized to
  /// baseline = min(min_score, MaxBaselineRatio)
  /// normalized_score = score - baseline
  /// ...
  ///  overall, p_match ~ 1 / (skew + score - baseline)
  template<
    typename ThreshRatio = std::ratio<-1, 1>,// we treat neg numerator as +infty
    typename SkewRatio = std::ratio<1, 10>,
    typename MaxBaselineRatio = std::ratio<1, 1>// treat neg numerator as +infty
  >
  struct RouletteSelector : public Selector {

    emp::Random & rand;

    RouletteSelector(emp::Random & rand_)
    : rand(rand_)
    { ; }

    emp::vector<size_t> operator()(
      emp::vector<size_t>& uids,
      std::unordered_map<size_t, double>& scores,
      size_t n
    ) {

      const double skew = ((double) SkewRatio::num / SkewRatio::den);
      emp_assert(skew > 0);

      // treat any negative numerator as positive infinity
      const double thresh = (
        ThreshRatio::num < 0
        ? std::numeric_limits<double>::infinity()
        : ((double) ThreshRatio::num) / ThreshRatio::den
      );

      // treat any negative numerator as positive infinity
      const double max_baseline = (
        MaxBaselineRatio::num < 0
        ? std::numeric_limits<double>::infinity()
        : ((double) MaxBaselineRatio::num) / MaxBaselineRatio::den
      );

      // partition by thresh
      size_t partition = 0;
      double min_score = std::numeric_limits<double>::infinity();
      for (size_t i = 0; i < uids.size(); ++i) {
        emp_assert(scores[uids[i]] >= 0);
        min_score = std::min(min_score, scores[uids[i]]);
        if (scores[uids[i]] <= thresh) {
          std::swap(uids[i], uids[partition++]);
        }
      }

      // skew relative to strongest match less than or equal to max_baseline
      // to take into account regulation...
      // (the default value of max_baseline is 1.0 because without
      // upregulation, the best possible match score is 1.0)
      const double baseline = std::min(min_score, max_baseline);
      emp_assert(baseline >= 0);
      emp_assert(baseline <= max_baseline);

      IndexMap match_index(partition);

      for (size_t p = 0; p < partition; ++p) {
        emp_assert(scores[uids[p]] - baseline >= 0);
        match_index.Adjust(p, 1.0 / ( skew + scores[uids[p]] - baseline ));
      }

      emp::vector<size_t> res;
      res.reserve(n);

      for (size_t j = 0; j < n; ++j) {
        const double match_pos = rand.GetDouble(match_index.GetWeight());
        const size_t idx = match_index.Index(match_pos);
        res.push_back(uids[idx]);
      }

      return res;
    }

  };

  struct DynamicSelector : public Selector {

    emp::vector<emp::Ptr<Selector>> selectors;

    size_t mode{0};

    emp::vector<size_t> operator()(
      emp::vector<size_t>& uids,
      std::unordered_map<size_t, double>& scores,
      size_t n
    ) {
      emp_assert(mode < selectors.size());
      return (*selectors[mode])(uids, scores, n);
    }

    ~DynamicSelector() {
      for (auto &ptr : selectors) ptr.Delete();
    }

  };

  /// A data container that allows lookup by tag similarity. It can be templated
  /// on different tag types and is configurable on construction for (1) the
  /// distance metric used to compute similarity between tags and (2) the
  /// selector that is used to select which matches to return. Regulation
  /// functionality is also provided, allowing dynamically adjustment of match
  /// strength to a particular item (i.e., making all matches stronger/weaker).
  /// A unique identifier is generated upon tag/item placement in the container.
  /// This unique identifier can be used to view or edit the stored items and
  /// their corresponding tags. Tag-based lookups return a list of matched
  /// unique identifiers.
  template <typename Val, typename Metric, typename Selector> class MatchBin {

  public:
    using tag_t = typename Metric::tag_t;
    using query_t = typename Metric::query_t;
    using uid_t = size_t;

  private:
    std::unordered_map<uid_t, Val> values;
    std::unordered_map<uid_t, double> regulators;
    std::unordered_map<uid_t, tag_t> tags;
    emp::vector<uid_t> uids;
    uid_t uid_stepper;

  public:
    Metric metric;
    Selector selector;

    MatchBin() : uid_stepper(0) { ; }

    MatchBin(emp::Random & rand)
    : uid_stepper(0)
    , selector(rand)
    { ; }

    /// Compare a query tag to all stored tags using the distance metric
    /// function and return a vector of unique IDs chosen by the selector
    /// function.
    emp::vector<uid_t> Match(const query_t & query, size_t n=1) {

      // compute distance between query and all stored tags
      std::unordered_map<tag_t, double> matches;
      for (const auto &[uid, tag] : tags) {
        if (matches.find(tag) == matches.end()) {
          matches[tag] = metric(query, tag);
        }
      }

      // apply regulation to generate match scores
      std::unordered_map<uid_t, double> scores;
      for (auto uid : uids) {
        scores[uid] = matches[tags[uid]] * regulators[uid] + regulators[uid];
      }
      return selector(uids, scores, n);
    }

    /// Put an item and associated tag in the container. Returns the uid for
    /// that entry.
    uid_t Put(const Val & v, const tag_t & t) {

      const uid_t orig = uid_stepper;
      while(values.find(++uid_stepper) != values.end()) {
        // if container is full
        // i.e., wrapped around because all uids already allocated
        if (uid_stepper == orig) throw std::runtime_error("container full");
      }

      values[uid_stepper] = v;
      regulators[uid_stepper] = 1.0;
      tags[uid_stepper] = t;
      uids.push_back(uid_stepper);
      return uid_stepper;
    }


    /// Delete an item and its associated tag.
    void Delete(const uid_t uid) {
      values.erase(uid);
      regulators.erase(uid);
      tags.erase(uid);
      uids.erase(std::remove(uids.begin(), uids.end(), uid), uids.end());
    }

    /// Clear all items and tags.
    void Clear() {
      values.clear();
      regulators.clear();
      tags.clear();
      uids.clear();
    }

    /// Access a reference single stored value by uid.
    Val & GetVal(const uid_t uid) {
      return values.at(uid);
    }

    /// Access a reference to a single stored tag by uid.
    tag_t & GetTag(const uid_t uid) {
      return tags.at(uid);
    }

    /// Generate a vector of values corresponding to a vector of uids.
    emp::vector<Val> GetVals(const emp::vector<uid_t> & uids) {
      emp::vector<Val> res;
      std::transform(
        uids.begin(),
        uids.end(),
        std::back_inserter(res),
        [this](uid_t uid) -> Val { return GetVal(uid); }
      );
      return res;
    }

    /// Generate a vector of tags corresponding to a vector of uids.
    emp::vector<tag_t> GetTags(const emp::vector<uid_t> & uids) {
      emp::vector<tag_t> res;
      std::transform(
        uids.begin(),
        uids.end(),
        std::back_inserter(res),
        [this](uid_t uid) -> tag_t { return GetTag(uid); }
      );
      return res;
    }

    /// Get the number of items stored in the container.
    size_t Size() {
      return values.size();
    }

    /// Add an amount to an item's regulator value. Positive amounts
    /// downregulate the item and negative amounts upregulate it.
    void AdjRegulator(uid_t uid, double amt) {
      regulators[uid] = std::max(0.0, regulators.at(uid) + amt);
    }

    /// Set an item's regulator value. Provided value must be greater than or
    /// equal to zero. A value between zero and one upregulates the item, a
    /// value of exactly one is neutral, and a value greater than one
    /// upregulates the item.
    void SetRegulator(uid_t uid, double amt) {
      emp_assert(amt >= 0.0);
      regulators.at(uid) = amt;
    }

  };

}

#endif
