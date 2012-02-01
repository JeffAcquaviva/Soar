#ifndef MODEL_H
#define MODEL_H

#include <iostream>
#include <fstream>
#include <string>
#include <map>
#include <vector>
#include <list>
#include "common.h"

class model {
public:
	model(const std::string &name, const std::string &type);
	virtual ~model() {}
	
	void init();
	void finish();
	
	std::string get_name() const {
		return name;
	}
	
	std::string get_type() const {
		return type;
	}
	
	virtual bool predict(const evec &x, evec &y) = 0;
	virtual int get_input_size() const = 0;
	virtual int get_output_size() const = 0;
	
	virtual float test(const evec &x, const evec &y);
	virtual void learn(const evec &x, const evec &y, float dt) {}
	virtual void save(std::ostream &os) const {}
	virtual void load(std::istream &is) {}
	
private:
	std::string name, type, path;
	std::ofstream predlog;
};

/*
 This class keeps track of how to combine several distinct models to make
 a single prediction.  Its main responsibility is to map the values from
 a single scene vector to the vectors that the individual models expect
 as input, and then map the values of the output vectors from individual
 models back into a single output vector for the entire scene. The mapping
 is specified by the Soar agent at runtime using the assign-model command.
 
 SVS uses a single instance of this class to make all its predictions. I
 may turn this into a singleton in the future.
*/
class multi_model {
public:
	multi_model();
	~multi_model();
	
	bool predict(const evec &x, evec &y);
	void learn(const evec &x, const evec &y, float dt);
	float test(const evec &x, const evec &y);
	
	std::string assign_model (
		const std::string &name, 
		const std::vector<std::string> &inputs, bool all_inputs,
		const std::vector<std::string> &outputs, bool all_outputs );

	void unassign_model(const std::string &name);
	
	void add_model(const std::string &name, model *m) {
		model_db[name] = m;
	}
	
	void set_property_vector(const std::vector<std::string> &props) {
		prop_vec = props;
	}
	
private:
	bool find_indexes(const std::vector<std::string> &props, std::vector<int> &indexes);

	struct model_config {
		std::string name;
		std::vector<std::string> xprops;
		std::vector<std::string> yprops;
		std::vector<int> xinds;
		std::vector<int> yinds;
		bool allx;
		bool ally;
		model *mdl;
	};
	
	std::list<model_config*>      active_models;
	std::map<std::string, model*> model_db;
	std::vector<std::string>      prop_vec;
};

#endif
