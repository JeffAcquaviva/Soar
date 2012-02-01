#include <assert.h>
#include <math.h>
#include <iostream>
#include <fstream>
#include <vector>
#include <algorithm>
#include <iterator>
#include <armadillo>
#include "nn.h"
#include "common.h"
#include "linear.h"
#include "soar_interface.h"
#include "scene.h"
#include "model.h"

using namespace std;
using namespace arma;

evec norm_vec(const evec &v, const evec &min, const evec &range) {
	return ((v.array() - min.array()) / range.array()).matrix();
}

class lwr : public model {
public:
	lwr(int nnbrs, const string &logpath, const string &name)
	: model(name, "lwr"), nnbrs(nnbrs), xsize(-1), ysize(-1), normalized(false)
	{
		nn = new brute_nn(&xnorm);
		if (!logpath.empty()) {
			log = new ofstream(logpath.c_str());
		} else {
			log = NULL;
		}
		init();
	}
	
	~lwr() {
		finish();
		if (log) {
			log->close();
			delete log;
		}
	}
	
	void learn(const evec &x, const evec &y, float dt) {
		if (xsize < 0) {
			xsize = x.size();
			ysize = y.size();
			xmin.resize(xsize);
			xmax.resize(xsize);
			xrange.resize(xsize);
			
			if (log) {
				(*log) << "@relation model" << endl;
				for (int i = 0; i < xsize; ++i) {
					(*log) << "@attribute x" << i << " real" << endl;
				}
				for (int i = 0; i < ysize; ++i) {
					(*log) << "@attribute y" << i << " real" << endl;
				}
				(*log) << "@data" << endl;
			}
		}
		
		if (log) {
			(*log) << x << y << endl;
		}
		
		examples.push_back(make_pair(x, y));
		if (examples.size() == 1) {
			xmin = x;
			xmax = x;
			xrange.setZero();
		} else {
			for (int i = 0; i < xsize; ++i) {
				if (x[i] < xmin[i]) {
					xmin[i] = x[i];
					normalized = false;
				} else if (x[i] > xmax[i]) {
					xmax[i] = x[i];
					normalized = false;
				}
			}
		}
		
		if (normalized) {
			// otherwise just wait for renormalization
			xnorm.push_back(norm_vec(x, xmin, xrange));
		}
	}
	
	bool predict(const evec &x, evec &y) {
		mat X, Y;
		vec d;
		di_queue neighbors;
		timer tall, tnn;
		int i, j, k;
	
		tall.start();
		k = examples.size() > nnbrs ? nnbrs : examples.size();
		if (k == 0) {
			return false;
		}
		
		if (!normalized) {
			normalize();
			normalized = true;
		}
		
		evec xn = norm_vec(x, xmin, xrange);
		
		tnn.start();
		nn->query(xn, k, neighbors);
		//cout << "NN:  " << tnn.stop() << endl;
		
		X.reshape(k, xsize);
		Y.reshape(k, ysize);
		d.reshape(k, 1);
		for(i = 0; i < k; ++i) {
			d(i) = neighbors.top().first;
			int ind = neighbors.top().second;
			neighbors.pop();
			evec &tx = examples[ind].first;
			evec &ty = examples[ind].second;
			for(j = 0; j < xsize; ++j) {
				X(i, j) = tx[j];
			}
			for(j = 0; j < ysize; ++j) {
				Y(i, j) = ty[j];
			}
		}
		
		vec w = sqrt(pow(d, -3));
		
		/*
		 Any neighbor whose weight is infinity is close enough
		 to provide an exact solution.	If any exist, take their
		 average as the solution.  If we don't do this the solve()
		 will fail due to infinite values in Z and V.
		*/
		rowvec closeavg = zeros<rowvec>(ysize);
		int nclose = 0;
		for (i = 0; i < w.n_elem; ++i) {
			if (w(i) == INFINITY) {
				closeavg += Y.row(i);
				++nclose;
			}
		}
		if (nclose > 0) {
			for(i = 0; i < closeavg.n_elem; ++i) {
				y[i] = closeavg(i) / nclose;
			}
			return true;
		}

		rowvec xv(x.size());
		for (i = 0; i < xv.n_cols; ++i) {
			xv(i) = x[i];
		}
		y[0] = pcr(X, Y, xv);
		return true;
	}
	
	int size() const {
		return examples.size();
	}
	
	void load(istream &is) {
		int nexamples;
		if (!(is >> xsize >> ysize >> nexamples)) {
			assert(false);
		}
		
		evec x(xsize), y(ysize);
		
		for (int i = 0; i < nexamples; ++i) {
			for(int j = 0; j < xsize; ++j) {
				if (!(is >> x[j])) {
					assert(false);
				}
			}
			for(int j = 0; j < ysize; ++j) {
				if (!(is >> y[j])) {
					assert(false);
				}
			}
			
			learn(x, y, 1);
		}
	}
	
	void save(ostream &os) const {
		vector<pair<evec, evec> >::const_iterator i;
		os << xsize << " " << ysize << " " << examples.size() << endl;
		for (i = examples.begin(); i != examples.end(); ++i) {
			for (int j = 0; j < xsize; ++j) {
				os << i->first[j] << " ";
			}
			for (int j = 0; j < ysize; ++j) {
				os << i->second[j] << " ";
			}
			os << endl;
		}
	}
	
	int get_input_size() const {
		return xsize;
	}
	
	int get_output_size() const {
		return ysize;
	}
	
private:
	void normalize() {
		vector<pair<evec, evec> >::iterator i;
		
		xrange = xmax - xmin;
		// can't have division by 0
		for (int i = 0; i < xrange.size(); ++i) {
			if (xrange[i] == 0.0) {
				xrange[i] = 1.0;
			}
		}
		xnorm.clear();
		xnorm.reserve(examples.size());
		for (i = examples.begin(); i != examples.end(); ++i) {
			xnorm.push_back(norm_vec(i->first, xmin, xrange));
		}
	}
	
	int xsize, ysize, nnbrs;
	std::vector<std::pair<evec, evec> > examples;
	std::vector<evec> xnorm;
	evec xmin, xmax, xrange;
	bool normalized;
	nearest_neighbor *nn;
	std::ofstream *log;
};

model *_make_lwr_model_(soar_interface *si, Symbol *root, scene *scn, const string &name) {
	Symbol *input_root = NULL, *output_root = NULL, *attr;
	wme_list children;
	wme_list::iterator i;
	long nnbrs = 50;
	string attrstr, logpath;
	wme *logwme;
	
	si->get_child_wmes(root, children);
	for (i = children.begin(); i != children.end(); ++i) {
		attr = si->get_wme_attr(*i);
		if (!si->get_val(attr, attrstr)) {
			continue;
		}
		if (attrstr == "num-neighbors") {
			if (!si->get_val(si->get_wme_val(*i), nnbrs)) {
				cerr << "WARNING: attribute num-neighbors does not have integer value, using default 50 neighbors" << endl;
			}
		}
	}
	
	if (si->find_child_wme(root, "log", logwme)) {
		si->get_val(si->get_wme_val(logwme), logpath);
	}
	
	return new lwr(nnbrs, logpath, name);
}
