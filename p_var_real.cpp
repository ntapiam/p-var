// compute p-variation of real-valued functions
// Authors:
// Vygantas Butkus <Vygantas.Butkus@gmail.com>
// Pavel Zorin-Kranich <pzorin@uni-bonn.de>

#include <cmath>
#include <list>

#include "p_var_real.h"

namespace p_var_real {
	// -------------------------------- definitions of types  ---------------------------------- //
	struct pointdata {
		size_t prev, next; // emulate double-linked list
		double pvdiff; // Difference to previous
	};
	
	typedef std::vector<pointdata> DoublyLinkedList;    // list of admissible points, in form of forward and backward links
	
	struct pvtemppoint{
		size_t it;
		double ev;
	};
	
	
	// the difference used in p-variation, i.e. the abs power of diff.
	double pvar_diff(double diff, double p){
		return std::pow(std::abs(diff), p);
	}
	
	// find local extrema and put them in a doubly linked list
	void DetectLocalExtrema(const NumericVector& x, DoublyLinkedList & links, const double p){
		size_t last_extremum = 0;
		int direction = 0;
		bool new_extremum = false;
		size_t n = x.size();
		
		links.front().prev = 0;
		links.front().pvdiff = 0.0;
		links.back().next = n;
		
		for(size_t i = 0 ; i<n ; i++) {
			size_t j = i+1;
			if (j != n) {
				if (x[j]>x[i]){
					new_extremum = (direction == -1);
					direction = 1;
				} else if(x[j]<x[i]) {
					new_extremum = (direction == 1);
					direction = -1;
				} else {
					new_extremum = false;
				}
			} else {
				// last point is extremal
				new_extremum = true;
			}
			if (new_extremum) {
				links[last_extremum].next = i;
				links[i].prev = last_extremum;
				links[i].pvdiff = pvar_diff(x[i] - x[last_extremum], p);
				last_extremum = i;
			}
		}
	}
	
	// Make sure that all intervals of length 3 are optimal
	void CheckShortIntervals(const NumericVector& x, DoublyLinkedList & links, const double& p){
		// Main principle:
		// if |pt[i] - pt[i+ d]|^p > sum_{j={i+1}}^d   |pt[j] - pt[j-1]|^p
		// but all shorter intervals are optimal,
		// then all middle points (i.e. p[j], j=i+1,...,i+d-1) are redundant
		
		double csum = 0;
		double fjoinval;
		
		size_t int_begin, int_end;
		int_begin = int_end = 0;
		for (size_t dcount = 0; dcount<3; dcount++) {
			int_end = links[int_end].next;
			if (int_end == x.size()) {
				return; // no intervals of this length
			}
			csum += links[int_end].pvdiff;
		}
		
		while ( true ) {
			fjoinval = pvar_diff(x[int_begin] - x[int_end], p);
			if (csum >= fjoinval){ // mid points are significant, continue to next interval
				int_end = links[int_end].next;
				if (int_end == x.size()) {
					return; // no next interval
				}
				int_begin = links[int_begin].next;
				csum -= links[int_begin].pvdiff;
				csum += links[int_end].pvdiff;
			} else { // mid points are redundant, erase them
				links[int_begin].next = int_end;
				links[int_end].prev = int_begin;
				int_begin = int_end;
				links[int_end].pvdiff = fjoinval;
				// backtrack, because we don't know if the changed intervals are significant
				csum = 0;
				for (size_t dcount = 0; dcount<3; dcount++) {
					if (int_begin >  0) {
						csum += links[int_begin].pvdiff;
						int_begin = links[int_begin].prev;
					} else {
						int_end = links[int_end].next;
						if (int_end == x.size()) {
							return; // no next interval
						}
						csum += links[int_end].pvdiff;
					}
				}
			}
		}
	}
	
	// Merge optimal intervals. LSI is the length of optimal intervals in the beginning.
	void MergeIntervalsRecursively(const NumericVector& x, DoublyLinkedList & links, const double& p, const size_t LSI=2){
		
		// Main principle:
		// 1. Put endpoints of optimal intervals in IterList
		// 2. Merge pairs of adjacent intervals using the function Merge2GoodInt. Repeat until all intervals are merged.
		
		// temporary data used by Merge2GoodInt. Declaring it here avoids allocating/deallocating it at each call of Merge2GoodInt.
		std::vector<pvtemppoint> av_mins, av_maxs, vb_mins, vb_maxs;
		std::vector<pvtemppoint>::iterator ait, bit, tait, tbit, sbit;
		
		
		// merge two intervals ([a, v] and [v, b]) which are known to be good.
		// Captures temporary variables by reference to avoid multiple allocation
		auto Merge2GoodInt = [&](size_t a, size_t v, size_t b){
			
			// Main principle:
			// 1. Find potential points in intervals [a,v) and (v, b]
			//    (i.e. the points that could make a new f-joint with any point form opposite interval).
			//    Those points are find using cummin and cummac starting from v.
			//     Some points might be dropped out before actual checking, but experiment showed, that it is not worthwhile.
			// 2. Sequentially check all possible joints. If any increase is detected, then all middle points are insignificant.
			
			if (a==v || v==b) return ; // nothing to calculate, exit the procedure.
			
			double amin, amax, bmin, bmax, ev, balance, maxbalance, fjoin, takefjoin;
			size_t prt_it;
			pvtemppoint pvtp;
			
			// 1. ### Find potential points
			av_mins.clear();
			av_maxs.clear();
			vb_mins.clear();
			vb_maxs.clear();
			
			// --- in interval [a,v) (starting from v).
			ev = 0;
			prt_it = v;
			amin = amax = x[v];
			while(prt_it!=a){
				ev += links[prt_it].pvdiff;
				prt_it = links[prt_it].prev;
				if(x[prt_it]>amax){
					amax=x[prt_it];
					pvtp.it = prt_it;
					pvtp.ev = ev;
					av_maxs.push_back (pvtp);
				}
				if(x[prt_it]<amin){
					amin = x[prt_it];
					pvtp.it = prt_it;
					pvtp.ev = ev;
					av_mins.push_back (pvtp);
				}
			}
			
			// --- in interval (v,b] (starting from v).
			ev = 0;
			prt_it = v;
			bmin = bmax = x[v];
			while(prt_it!=b){
				prt_it = links[prt_it].next;
				ev += links[prt_it].pvdiff;
				if(x[prt_it]>bmax){
					bmax = x[prt_it];
					pvtp.it = prt_it;
					pvtp.ev = ev;
					vb_maxs.push_back (pvtp);
				}
				if( x[prt_it]<bmin){
					bmin = x[prt_it];
					pvtp.it = prt_it;
					pvtp.ev = ev;
					vb_mins.push_back (pvtp);
				}
			}
			
			// 2. ### Sequentially check all possible joints: finding the best i,j \in [a, v)x(v,b] that could be joined
			takefjoin = 0;
			maxbalance = 0;
			sbit = vb_maxs.begin();
			for(ait=av_mins.begin(); ait!=av_mins.end(); ait++){
				for(bit=sbit; bit!=vb_maxs.end(); bit++){
					fjoin = pvar_diff( x[(*ait).it] - x[(*bit).it], p );
					balance = fjoin - (*bit).ev - (*ait).ev ;
					if (balance>maxbalance){
						maxbalance = balance;
						takefjoin = fjoin;
						tait = ait;
						sbit = tbit = bit;
					}
				}
			}
			
			sbit = vb_mins.begin();
			for(ait=av_maxs.begin(); ait!=av_maxs.end(); ait++){
				for(bit=sbit; bit!=vb_mins.end(); bit++){
					fjoin = pvar_diff( x[(*ait).it] - x[(*bit).it], p );
					balance = fjoin - (*bit).ev - (*ait).ev ;
					if (balance>maxbalance){
						maxbalance = balance;
						takefjoin = fjoin;
						tait = ait;
						sbit = tbit = bit;
					}
				}
			}
			
			// if we found any point, join it by erasing all middle points
			if(maxbalance>0){
				links[(*tait).it].next = (*tbit).it;
				links[(*tbit).it].prev = (*tait).it;
				links[(*tbit).it].pvdiff = takefjoin;
			}
		};
		
		size_t it = 0;
		std::list<size_t> IterList;
		std::list<size_t>::iterator a_IL, v_IL, b_IL;
		
		// 1. ### Finding all the intervals that will be merged
		int count = 0;
		while(it < x.size()){
			if(count % LSI == 0){
				IterList.push_back (it);
			}
			++count;
			it = links[it].next;
		}
		IterList.push_back (x.size()-1);
		
		// ### 2. Merging pairs of interval until everything is merged.
		while(IterList.size()>2){
			a_IL = v_IL = IterList.begin();
			++v_IL;
			b_IL = v_IL;
			++b_IL;
			while((b_IL!=IterList.end()) & (v_IL!=IterList.end())){
				Merge2GoodInt(*a_IL, *v_IL, *b_IL);
				a_IL = IterList.erase(v_IL);
				v_IL = a_IL;
				++v_IL;
				b_IL = v_IL;
				++b_IL;
			}
		}
	}
	
	// p-variation calculation (in C++)
	double pvar(const NumericVector& x, double p) {
		
		// short special cases
		if (x.size() <= 2) {
			if (x.size() <= 1) {
				return 0;
			} else {
				return std::pow(std::abs(x[0]-x[1]), p);
			}
		}
		
		DoublyLinkedList links(x.size());
		DetectLocalExtrema(x, links,  p);
		CheckShortIntervals(x, links, p);
		MergeIntervalsRecursively(x, links, p, 4);
		
		// output:
		double pvalue=0;
		size_t i = 0;
		while ( i < x.size() ){
			pvalue += links[i].pvdiff;
			i = links[i].next;
		}
		
		return pvalue;
	}
} // namespace p_var_real
