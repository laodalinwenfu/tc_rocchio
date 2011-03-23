/*****************************************************************************
 * Copyright (C) 2011  Tadeus Prastowo (eus@member.fsf.org)                  *
 *                                                                           *
 * This program is free software: you can redistribute it and/or modify      *
 * it under the terms of the GNU General Public License as published by      *
 * the Free Software Foundation, either version 3 of the License, or         *
 * (at your option) any later version.                                       *
 *                                                                           *
 * This program is distributed in the hope that it will be useful,           *
 * but WITHOUT ANY WARRANTY; without even the implied warranty of            *
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the             *
 * GNU General Public License for more details.                              *
 *                                                                           *
 * You should have received a copy of the GNU General Public License         *
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.     *
 *****************************************************************************/

#ifndef UTILITY_THRESHOLD_ESTIMATION_HPP
#define UTILITY_THRESHOLD_ESTIMATION_HPP

#include <cassert>
#include <vector>
#include <string>
#include <map>
#include "utility.hpp"
#include "utility_classifier.hpp"
#include "utility_doc_cat_list.hpp"
#include "rocchio.hpp"

using namespace std;

#ifdef BE_VERBOSE
class class_d_list_entry {
public:
  unsigned int first;
  unsigned int second;
  vector <string *> first_docs;
  vector <string *> second_docs;
  class_d_list_entry() {
    first = 0;
    second = 0;
  }
};
#else
typedef pair<unsigned int /*|C|*/, unsigned int /*|~C|*/> class_d_list_entry;
#endif

typedef map<double /* dot product */, class_d_list_entry> class_d_list;

#ifdef BE_VERBOSE
static inline void print_bit(const class_d_list::const_reverse_iterator &bit,
			     unsigned int b, unsigned int c)
{
  const class_d_list_entry &e = bit->second;

  fprintf(stderr, "@ %f |C|=%05u |~C|=%05u (b=%05u c=%05u)\n",
	  bit->first, e.first, e.second, b, c);

  if ((e.first != 0 || e.second != 0)
      && e.first_docs.empty() && e.second_docs.empty()) { // In unit testing
    return;
  }

  fprintf(stderr, "\t C = {");
  for (vector<string *>::const_iterator x = e.first_docs.begin();
       x != e.first_docs.end();
       x++)
    {
      fprintf(stderr, " %s", (*x)->c_str());
    }
  fprintf(stderr, " }\n");

  fprintf(stderr, "\t~C = {");
  for (vector<string *>::const_iterator x = e.second_docs.begin();
       x != e.second_docs.end();
       x++)
    {
      fprintf(stderr, " %s", (*x)->c_str());
    }
  fprintf(stderr, " }\n");
}
#endif

/* Auxiliary function of function estimate_Th.
 * cat_doc_count is the number of unique documents that belong to target cat.
 */
static inline double do_threshold_estimation(unsigned int cat_doc_count,
					     const class_d_list &bit_string,
					     double &threshold)
{
  if (bit_string.empty()) { // Nothing to be estimated
    threshold = 0;
    return 1; // precision and recall are trivially 1 when |C| = 0 and b = 0
  }

  if (cat_doc_count == 0) {
    /* Cat has no doc.
     * This corresponds to the set of cases {0, 00, 000, ...}
     * So, the action mentioned in the explanation in function estimate_Th in
     * the case where all bits are zero is applied.
     */
    threshold = 1.5 * bit_string.rbegin()->first;
    return 1; // precision and recall are trivially 1 when |C| = 0 and b = 0
  }

  unsigned int b = 0;
  unsigned int c = cat_doc_count;
#ifdef BE_VERBOSE
  fprintf(stderr, "b=%05u c=%05u\n", b, c);
#endif
  class_d_list::const_reverse_iterator prev_bit = bit_string.rend();
  for (class_d_list::const_reverse_iterator bit = bit_string.rbegin();
       bit != bit_string.rend();
       bit++)
    {
      unsigned int next_c = c - bit->second.first;
      unsigned int next_b = b + bit->second.second;

#ifdef BE_VERBOSE
      print_bit(bit, next_b, next_c);
#endif

      if (next_b > next_c) {
	double prev_diff = (get_precision(cat_doc_count - c, b)
			    - get_recall(cat_doc_count - c, c));
	double curr_diff = (get_recall(cat_doc_count - next_c, next_c)
			    - get_precision(cat_doc_count - next_c, next_b));
#ifdef BE_VERBOSE
	fprintf(stderr, "prev_diff %f, curr_diff %f\n", prev_diff, curr_diff);
#endif
	if (prev_diff > curr_diff) {
	  b = next_b;
	  c = next_c;
	  threshold = bit->first;
#ifdef BE_VERBOSE
	  fprintf(stderr, "b > c: threshold := bit (%f)\n", threshold);
#endif
	} else {
	  threshold = prev_bit->first;
#ifdef BE_VERBOSE
	  fprintf(stderr, "b > c: threshold := prev_bit (%f)\n", threshold);
#endif
	}
	break;
      }	else if (next_b == next_c) {
	b = next_b;
	c = next_c;
	threshold = bit->first;
#ifdef BE_VERBOSE
	fprintf(stderr, "b = c: threshold := bit (%f)\n", threshold);
#endif
	break;
      } else {
	c -= bit->second.first;
	b += bit->second.second;
	prev_bit = bit;
      }
    }

  /* Interpolated BEP */
  double precision = get_precision(cat_doc_count - c, b);
  double recall = get_recall(cat_doc_count - c, c);

#ifdef BE_VERBOSE
  fprintf(stderr,
	  "a = %u, b = %u, c = %u\n"
	  "precision = %f, recall = %f\n",
	  cat_doc_count - c, b, c, precision, recall);
#endif

  return 0.5 * (precision + recall);
}

#ifndef NDEBUG
/* The test cases are taken from the explanation given in estimate_Th */
static inline void test_do_threshold_estimation(void)
{
  class_d_list bit_string;
  double Th = 0;
  double deviation;

#ifdef BE_VERBOSE
  fprintf(stderr, "*test_do_threshold_estimation():\n");
#endif

  /* 1) 1101001 can be divided into 1101 and 001 where b = c = 1 and a = 3 */
  bit_string[40].first += 1;
  bit_string[27].first += 1;
  bit_string[25].second += 1;
  bit_string[17].first += 1;
  bit_string[11].second += 1;
  bit_string[10].second += 1;
  bit_string[8].first += 1;
  deviation = do_threshold_estimation(4, bit_string, Th) - 0.75;
  assert(((deviation < 0) ? (-deviation) : deviation) < FP_COMPARISON_DELTA);
  deviation = Th - 17;
  assert(((deviation < 0) ? (-deviation) : deviation) < FP_COMPARISON_DELTA);
  bit_string.clear();
  Th = 0;

  /* 2) 1 can be divided into 1 and nothing where b = c = 0 and a = 1 */
  bit_string[100].first += 1;
  deviation = do_threshold_estimation(1, bit_string, Th) - 1;
  assert(((deviation < 0) ? (-deviation) : deviation) < FP_COMPARISON_DELTA);
  deviation = Th - 100;
  assert(((deviation < 0) ? (-deviation) : deviation) < FP_COMPARISON_DELTA);
  bit_string.clear();
  Th = 0;
  
  /* 3) 0 can be divided into nothing and 0 where b = c = 0 and a = 0 */
  bit_string[1].second += 1;
  deviation = do_threshold_estimation(0, bit_string, Th) - 1;
  assert(((deviation < 0) ? (-deviation) : deviation) < FP_COMPARISON_DELTA);
  deviation = Th - 1.5;
  assert(((deviation < 0) ? (-deviation) : deviation) < FP_COMPARISON_DELTA);
  bit_string.clear();
  Th = 0;

  /* 4) nothing can be divided into nothing and nothing where b = c = 0 */
  deviation = do_threshold_estimation(0, bit_string, Th) - 1;
  assert(((deviation < 0) ? (-deviation) : deviation) < FP_COMPARISON_DELTA);
  deviation = Th - 0;
  assert(((deviation < 0) ? (-deviation) : deviation) < FP_COMPARISON_DELTA);
  bit_string.clear();
  Th = 0;

  /* 5) Duplicated values:
   * 1100101100
   * 10 1 10 01
   *  1 0 1  1
   *  0 0 0  1
   */
  bit_string[9.3].first += 1;
  bit_string[9.3].first += 1;
  bit_string[8.9].first += 1;
  bit_string[8.9].second += 1;
  bit_string[8.9].first += 1;
  bit_string[8.9].second += 1;
  bit_string[8.5].second += 1;
  bit_string[8.1].second += 1;
  bit_string[8.1].first += 1;
  bit_string[8.1].second += 1;
  bit_string[8.1].second += 1;
  bit_string[4.5].first += 1;
  bit_string[3.4].second += 1;
  bit_string[3.4].first += 1;
  bit_string[3.4].first += 1;
  bit_string[3.4].second += 1;
  bit_string[3.1].first += 1;
  bit_string[3.1].second += 1;
  bit_string[2.1].first += 1;
  bit_string[1.9].second += 1;
  bit_string[1.9].second += 1;
  bit_string[1.9].first += 1;
  bit_string[1.9].first += 1;
  bit_string[1.3].second += 1;
  bit_string[1.3].first += 1;
  deviation = (do_threshold_estimation(13, bit_string, Th)
	       - 0.5 * (6.0/12.0 + 6.0/13.0));
  assert(((deviation < 0) ? (-deviation) : deviation) < FP_COMPARISON_DELTA);
  deviation = Th - 4.5;
  assert(((deviation < 0) ? (-deviation) : deviation) < FP_COMPARISON_DELTA);
  bit_string.clear();
  Th = 0;
}
#endif /* NDEBUG of test_do_threshold_estimation() */

/**
 * @param unique_docs the docs used to estimate the threshold
 * @param unique_docs_in_target_cat is used to check for category emptiness
 *
 * @return interpolated BEP associated with the estimated threshold
 */
static inline double estimate_Th(class_unique_docs_for_estimating_Th unique_docs,
				 const class_docs &unique_docs_in_target_cat,
				 const string &target_cat_name,
				 class_classifier &target_cat_classifier)
{
/*
The threshold estimation will use the BEP method so as to make the experiment results more comparable with those of the other researchers.

The goal of threshold estimation using BEP over a set S of labeled document weight vectors (each such a vector is called w) is to find the threshold (Th) of the profile vector W of a binary classifier h associated with category C such that the precision and the recall of h is equal over S. Specifically, a labeled document d associated with a w vector in S has either GS(d) = {C} if the document is labeled to be in C or GS(d) = {} if the document is labeled to be not in C. Then, for all w vectors in S, one performs a dot product between a w vector and the W vector. Next, based on the dot product values, the w vectors are grouped into two: group C and group ~C. Those that have dot product values greater than or equal to Th are in group C while those that are not are in group ~C. Note that the grouping depends on the value of Th. Once the grouping is done, any document d in group C will have h(d) = {C} while any document d in group ~C will have h(d) = {}. Having h(d) under a certain value of Th and GS(d) for any document in S, the precision and the recall can be calculated as explained in utility_doc_cat_list.hpp. The goal is then to find the value of Th that can make precision equal to recall over S. However, such a Th may not exist.

When such a Th does not exist, for example when several w vectors produce the same dot product value V with the W vector and setting Th to include V results in precision > recall while setting Th to not include V results in precision < recall, Th should be set in such a way so as to minimize the difference between precision and recall |precision - recall|. In the example, if including V results in |precision - recall| that is smaller than not including V, Th is set to include V. Otherwise, Th is set to not include V.

There are several ways to estimate the value of Th using BEP method. First, one can keep incrementing the threshold starting from 0 (i.e., from a perfect recall that means that recall = 1) up to the point where precision >= recall. This method, however, will take a long time. Let e be the value by which the threshold is incremented each time. Then, at worst it will take around (P_1 - 0) / e where P_1 is the greatest dot product value. So, a method like binary search can be employed.

A binary search between 0 and P_1 will at worst take log_2(P_1 - 0) * N where N is the cardinality of S. N is present because for each new threshold, one has to calculate the number of documents that are incorrectly classified to obtain precision and recall. Specifically, let a denotes the upper end point and b denotes the lower end point. Initially a = P_1 and b = 0. For each step, Th is set to (a + b) / 2 and precision and recall are evaluated. If precision = recall, the search halts. Otherwise before going to the next step, if precision > recall, a is set to Th while if precision < recall, b is set to Th. However, a binary search does not perform well when precision cannot be equal to recall.

Using the example on V above, sooner or later the binary search will have a = V and the distance between a and b is kept halved until the search halts due to the limited precision of double data type. In the absence of the limitation, the search will never halt since between any two real numbers that are different, there exists a set of real numbers. In other words, when precision cannot be equal to recall, a binary search will be in vain. Therefore, in order to avoid this problem and to have a better time complexity N, I implement the following search method.

My implementation will sort the dot product values uniquely. Without loss of generality, I will only consider the case of descending order sort. This will result in: P_1, ..., P_K where K the number of unique dot product values. Now let b be the number of documents d in which h(d) = {C} but GS(d) = {} and c be the number of documents d in which h(d) = {} but GS(d) = {C}. Initially b = 0 and c = |C| where |C| is the number of documents d having GS(d) = {C}. Then, my implementation walks from P_1 to P_N at each step of which b is incremented by the number of documents whose dot products result in P_1 but are incorrectly classified into C while c is decremented by the number of documents whose dot products result in P_1 and correctly classified into C. The walk stops at the dot product value P_i at which b >= c. If b = c, Th is set to P_i. Otherwise, the precisions and recalls at two points are evaluated. One point is when Th is set to P_i and the other is when Th is set to P_(i-1). Then, Th is set to the dot product value that results in the most minimal |precision - recall|.

The sole use of b and c in my method can be understood from the definitions of precision and recall that can be stated as:

  precision = recall
a / (a + b) = a / (a + c) ... definitions of precision and recall
1 / (a + b) = 1 / (a + c) ... dividing both sides by a
    (a + b) = (a + c)     ... multiplying both sides by (a + b) and (a + c)
          b = c           ... subtracting a from both sides

That is, precision = recall if and only if b = c. Therefore, when walking from P_1 to P_N, I only need to detect a flip in the value of b and c to decide at which step Th should lie. Based on the sole use of b and c, the problem of estimating Th using BEP can be formulated as the problem of counting binary bits.

First, sort the dot product values without omitting duplicated values. As before, without loss of generality I will only consider the case of descending sort. This will result in: B_1, ..., B_N where N is the cardinality of S and B_1 is the maximal dot-product value and consecutive pair-wise different elements are not necessarily unique (i.e., equivalent dot product values are simply put one after another like the twos in 1, 2, 2, 2, 3). Then, each value B_i is associated with a binary bit in which bit 1 means that the document d having the dot product value B_i has GS(d) = {C} while bit 0 means that GS(d) = {}. For example:
+----+----+----+----+
|P_1 |P_2 |P_3 |P_4 |
+----+----+----+----+
|1   |0   |1   |1   |
+----+----+----+----+
Then, the binary string can be divided into two parts such that the number of zeros in the first part, which signifies b, is equal to the number of ones in the second part, which signifies c. For example:
1) 1101001 can be divided into 1101 and 001 where b = c = 1
2) 1 can be divided into 1 and nothing where b = c = 0
3) 0 or 00 or 000 can be divided into nothing and 0 or 00 or 000 where b = c = 0
4) nothing can be divided into nothing and nothing where b = c = 0

The candidate Th then uses the dot product value of the right-most bit 1 on the left part. In Example 1, it is the dot product value Q associated with the right-most bit 1 of 1101. If Q is associated with only one document, then Th is set to Q. Otherwise, the precisions and recalls at Q and at the next larger dot product value are evaluated. Then, Th is set to the dot product value that has the most minimum |precision - recall|.

For the case where all bits are zero, Th should be a value that is larger than the largest dot product value. Regarding how larger it should be, assuming that most of the unseen bit 1s will be above bit 0s and the number of bit 1s is less than the number of bit 0s (|C| is usually less than |~C|), the right way to decide the threshold is based on the distribution of known bit 0s and use confidence interval. But, for now I will just set the threshold to 1.5 times of the largest dot product value. For example, in Example 3, the threshold is the dot product value V of bit 0 plus 0.5 * V.

If no bit exists like in Example 4, the threshold is 0.
*/

  if (unique_docs_in_target_cat.empty()) {
    /* Cat has no doc.
     * This corresponds to the set of cases {0, 00, 000, ...}
     * So, the action mentioned in the above explanation in the case where all
     * bits are zero is applied.
     */

    double max_dot_product = 0; // dot product w and W cannot be < 0
    for (class_unique_docs_for_estimating_Th::const_iterator i
	   = unique_docs.begin();
	 i != unique_docs.end();
	 i++)
      {
	double result
	  = dot_product_sparse_vector((*i)->first,
				      target_cat_classifier.second);
	if (result > max_dot_product) {
	  max_dot_product = result;
	}
      }
    
    target_cat_classifier.first.threshold = 1.5 * max_dot_product;
    return 1; // precision and recall are trivially 1 when |C| = 0 and b = 0
  }

  class_d_list d_list;

  /* Constructing the bits */
  unsigned int cat_doc_count = 0;
  for (class_unique_docs_for_estimating_Th::const_iterator i
	 = unique_docs.begin();
       i != unique_docs.end();
       i++)
    {
      double dot_prod
	= dot_product_sparse_vector((*i)->first, target_cat_classifier.second);
      class_d_list_entry &entry = d_list[dot_prod];

      class_set_of_cats *doc_GS = (*i)->second;
      int bit_belongs_to_target_cat = (doc_GS != NULL // doc is in excluded cat
				       && (doc_GS->find(target_cat_name)
					   != doc_GS->end()));

      if (bit_belongs_to_target_cat) {

	cat_doc_count++;
	entry.first++;

#ifdef BE_VERBOSE
	entry.first_docs.push_back(&w_to_doc_name[&(*i)->first]);
#endif
      } else {

	entry.second++;

#ifdef BE_VERBOSE
	entry.second_docs.push_back(&w_to_doc_name[&(*i)->first]);
#endif
      }
    }
  /* End of bits construction */

#ifdef BE_VERBOSE
  fprintf(stderr,
	  "The number of bits (i.e., unique dot product values) is %u\n"
	  "Threshold estimation on %s (c = %u = |C|, |~C| = %u)\n",
	  d_list.size(), target_cat_name.c_str(), cat_doc_count,
	  unique_docs.size() - cat_doc_count);
#endif
  
  double interpolated_BEP
    = do_threshold_estimation(cat_doc_count, d_list,
			      target_cat_classifier.first.threshold);

#ifdef BE_VERBOSE
  fprintf(stderr, "Interpolated BEP = %f\n", interpolated_BEP);
#endif

  return interpolated_BEP;
}

#ifndef NDEBUG
/* The test cases are taken from the explanation given in estimate_Th */
static inline void test_estimate_Th(void)
{
  class_doc_cat_list gold_standard;
  class_w_cats_list all_docs;
  class_unique_docs_for_estimating_Th unique_docs;
  class_docs unique_docs_in_target_cat;
  string target_cat_name("X");
  class_classifier target_cat_classifier;
  double deviation;

  target_cat_classifier.second[0] = 1;

#define __make_doc(cat_name, doc_name, first_w_entry) do {	\
    class_set_of_cats &cats = gold_standard[doc_name];		\
    cats.insert(cat_name);					\
    all_docs.push_back(class_w_cats());				\
    class_w_cats &doc = all_docs.back();			\
    doc.second = &cats;						\
    doc.first[0] = first_w_entry;				\
    unique_docs.push_back(&doc);				\
  } while (0)

#define __reset_test_case() do {		\
    gold_standard.clear();			\
    unique_docs_in_target_cat.clear();		\
    unique_docs.clear();			\
    all_docs.clear();				\
    target_cat_classifier.first.threshold = 0;	\
  } while (0)

#define __do_test(expected_interpolated_BEP, expected_Th) do {		\
    /* Check interpolated BEP */					\
    deviation = (estimate_Th(unique_docs,				\
			     unique_docs_in_target_cat,			\
			     target_cat_name,				\
			     target_cat_classifier)			\
		 - (expected_interpolated_BEP));			\
    assert(((deviation < 0)						\
	    ? (-deviation)						\
	    : deviation) < FP_COMPARISON_DELTA);			\
    /* Check Th */							\
    deviation = target_cat_classifier.first.threshold - (expected_Th);	\
    assert(((deviation < 0)						\
	    ? (-deviation)						\
	    : deviation) < FP_COMPARISON_DELTA);			\
  } while (0)

#ifdef BE_VERBOSE

  fprintf(stderr, "*test_estimate_Th():\n");

#define make_doc(cat_name, doc_name, first_w_entry) do {	\
    __make_doc(cat_name, doc_name, first_w_entry);		\
    class_w_cats &doc = all_docs.back();			\
    unique_docs_in_target_cat.insert(&doc.first);		\
    w_to_doc_name[&doc.first] = doc_name;			\
  } while (0)
#define reset_test_case() do {			\
    __reset_test_case();			\
    w_to_doc_name.clear();			\
  } while (0)
#define do_test(test_name, expected_interpolated_BEP, expected_Th) do {	\
    fprintf(stderr, "** " test_name ":\n");				\
    __do_test(expected_interpolated_BEP, expected_Th);			\
  } while (0)

#else /* !BE_VERBOSE */

#define make_doc(cat_name, doc_name, first_w_entry) do {	\
    __make_doc(cat_name, doc_name, first_w_entry);		\
    class_w_cats &doc = all_docs.back();			\
    unique_docs_in_target_cat.push_back(&doc.first);	\
  } while (0)
#define reset_test_case() do {			\
    __reset_test_case();			\
  } while (0)
#define do_test(test_name, expected_interpolated_BEP, expected_Th) do {	\
    __do_test(expected_interpolated_BEP, expected_Th);			\
  } while (0)
#endif /* !BE_VERBOSE */

#define make_empty_cat(cat_name) do {		\
    all_docs.push_back(class_w_cats());		\
    class_w_cats &doc = all_docs.back();	\
    doc.second = NULL;				\
    unique_docs_in_target_cat.clear();		\
  } while (0)

  /* 1) 1101001 can be divided into 1101 and 001 where b = c = 1 and a = 3 */
  make_doc("X", "d1", 7.3); // 1
  make_doc("X", "d2", 7.1); // 1
  make_doc("Y", "d3", 6.7); // 0
  make_doc("X", "d4", 5.4); // 1
  make_doc("Y", "d5", 5.1); // 0
  make_doc("Z", "d6", 4.9); // 0
  make_doc("X", "d7", 4.7); // 1
  do_test("Case 1", 0.75, 5.4);
  reset_test_case();

  /* 2) 1 can be divided into 1 and nothing where b = c = 0 and a = 1 */
  make_doc("X", "d1", 7.3); // 1
  do_test("Case 2", 1, 7.3);
  reset_test_case();

  /* 3) 0 can be divided into nothing and 0 where b = c = 0 and a = 0 */
  make_empty_cat("X"); // nothing
  make_doc("Y", "d1", 7.3);  // 0
  do_test("Case 3", 1, 1.5 * 7.3);
  reset_test_case();

  /* 4) nothing can be divided into nothing and nothing where b = c = 0 */
  make_empty_cat("X"); // nothing
  make_empty_cat("Y"); // nothing
  do_test("Case 4", 1, 0);
  reset_test_case();

  /* 5) Duplicated values:
   * 1100101100
   * 10 1 10 01
   *  1 0 1  1
   *  0 0 0  1
   */
  make_doc("X", "d1", 7.3); make_doc("X", "d2", 7.3);   // 11
  make_doc("X", "d3", 7.1); make_doc("Y", "d4", 7.1);   // 1010
  make_doc("X", "d5", 7.1); make_doc("Z", "d6", 7.1);
  make_doc("Y", "d7", 5.8);                             // 0
  make_doc("W", "d8", 5.4); make_doc("X", "d9", 5.4);   // 0100
  make_doc("Z", "d10", 5.4); make_doc("Z", "d11", 5.4);
  make_doc("X", "d12", 5.2);                            // 1
  make_doc("Z", "d13", 4.7); make_doc("X", "d14", 4.7); // 0110
  make_doc("X", "d15", 4.7); make_doc("Y", "d16", 4.7);
  make_doc("X", "d17", 4.2); make_doc("W", "d18", 4.2); // 10
  make_doc("X", "d19", 4.1);                            // 1
  make_doc("W", "d20", 3.1); make_doc("Y", "d21", 3.1); // 0011
  make_doc("X", "d22", 3.1); make_doc("X", "d23", 3.1);
  make_doc("Z", "d24", 3); make_doc("X", "d25", 3);     // 01
  do_test("Case 5", 0.5 * (6.0/12.0 + 6.0/13.0), 5.2);
  reset_test_case();

#undef reset_test_case
#undef make_doc
#undef make_empty_cat
#undef do_test
#ifdef BE_VERBOSE
  #undef __make_doc
  #undef __do_test
  #undef __reset_test_case
#endif
}
#endif /* NDEBUG of test_test_estimate_Th() */

#endif /* UTILITY_THRESHOLD_ESTIMATION_HPP */