/*
 *  obj2a2m - Alias Wavefront .obj -> A2E .a2m Converter
 *  Copyright (C) 2004 - 2010 Florian Ziesche
 *  
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; version 2 of the License only.
 *  
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *  
 *  You should have received a copy of the GNU General Public License along
 *  with this program; if not, write to the Free Software Foundation, Inc.,
 *  51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#include "obj2a2m.h"

/*!
 * \mainpage
 *
 * \author flo
 *
 * \date v1: August 2004 - November 2005 /// v2: January - February 2006, renewed: April 2009 /// v3: April - May 2009, September 2010, April 2012
 *
 * Albion 2 Engine Tool - Alias Wavefront .obj -> A2E .a2m Converter
 *
 *
 * little specification of the A2E static model format:
 *
 * [A2EMODEL - 8 bytes]
 * [VERSION - 4 bytes (unsigned int) = 0x00000002]
 * [TYPE - 1 byte (0x00 = no collision model, 0x02 = has a collision model)]
 * [VERTEX COUNT - 4 bytes]
 * [TEXTURE COORDINATE COUNT - 4 bytes]
 * [VERTICES - 4 bytes * 3 * VERTEX COUNT]
 * [TEXTURE COORDINATES - 4 bytes * 2 * TEXTURE COORDINATE COUNT]
 * [OBJECT COUNT - 4 bytes]
 * [OBJECT NAMES - OBJECT COUNT *  x bytes (names are separated by 0xFF)]
 * [FOR EACH OBJECT COUNT]
 * 		[INDEX COUNT - 4 bytes]
 * 		[INDICES - 4 bytes * 3 * INDEX COUNT]
 * 		[TEXTURE INDICES - 4 bytes * 3 * INDEX COUNT]
 * [END FOR]
 * [IF TYPE == 0x00] [END OF MODEL]
 * [ELSE] (TYPE == 0x02 / collision model)
 * 		[VERTEX COUNT - 4 bytes]
 * 		[VERTICES - 4 bytes * 3 * VERTEX COUNT]
 * 		[INDEX COUNT - 4 bytes]
 * 		[INDICES - 4 bytes * 3 * INDEX COUNT]
 * 		[END OF MODEL]
 */

pair<string, string> get_face_indices(string face_str) {
	string vertex_index, coord_index;
	size_t first_slash, last_slash;
	// check if face string contains a '/', if not, only a vertex index is specified
	if((first_slash = face_str.find("/")) != string::npos) {
		vertex_index = face_str.substr(0, first_slash);
		
		// check for second '/', this happens if there is also a normal index specified
		last_slash = face_str.find_last_of("/");
		if(first_slash == last_slash-1) {
			coord_index = "1";
		}
		else if(first_slash != last_slash) {
			coord_index = face_str.substr(first_slash+1, last_slash - first_slash - 1);
		}
		else {
			coord_index = face_str.substr(first_slash+1, face_str.length() - first_slash);
		}
	}
	else {
		vertex_index = face_str;
		coord_index = "1";
		a2e_error("face contains no texture coordinate index - using \"1\"!");
	}
	
	return pair<string, string>(vertex_index, coord_index);
}

inline bool is_number(string str) {
	if(!(str[0] >= '0' && str[0] <= '9') && str[0] != '-') return false;
	return true;
}

bool load_obj_data(bool collision_obj, const char* filename, vector<float3*>* vertices, vector<coord*>* tex_coords, map<unsigned int, vector<s_index*>>* indices, map<unsigned int, vector<s_index*>>* tex_indices, map<unsigned int, string>* obj_names, map<unsigned int, string>* obj_mats) {
	int cur_subobj = -1;
	bool uvw_texcoord = true;
	bool quad_face = false;
	bool init_uvw_check = false;
	bool set_word = false;
	
	// read and store obj data
	file_io f(filename, file_io::OT_READ_BINARY);
	if(!f.is_open()) {
		a2e_error("couldn't open obj file \"%s\"!", filename);
		return false;
	}
	
	stringstream buffer(stringstream::in | stringstream::out);
	core::reset(&buffer);
	f.read_file(&buffer);
	f.close();
	
	buffer.seekp(0);
	buffer.seekg(0);
	
	string cur_word, val1, val2, val3, val4;
	cur_word.reserve(256);
	val1.reserve(256);
	val2.reserve(256);
	val3.reserve(256);
	val4.reserve(256);
	
	map<string, size_t> object_mats;
	
	buffer >> cur_word;
	bool is_word = !buffer.fail();
	while(is_word && !buffer.eof()) {
		// vertex
		if(cur_word == "v") {
			buffer >> val1;
			buffer >> val2;
			buffer >> val3;
			
			vertices->push_back(new float3(string2float(val1), string2float(val2), string2float(val3)));
		}
		// texture coordinate
		else if(cur_word == "vt") {
			buffer >> val1;
			buffer >> val2;
			
			// some .obj files use uvw texture coordinates instead of uv coordinates, do a check at the first occurence of vt
			if(!init_uvw_check) {
				string next_val = buffer.str().substr(1 + buffer.tellg(), 4);
				if(!is_number(next_val)) {
					uvw_texcoord = false;
				}
				init_uvw_check = true;
			}
			
			if(uvw_texcoord) buffer >> val3; // ignored
			
			tex_coords->push_back(new coord());
			tex_coords->back()->u = string2float(val1);
			tex_coords->back()->v = string2float(val2);
		}
		// normal - ignore
		else if(cur_word == "vn") {
		}
		// smooth group - ignore
		else if(cur_word == "s") {
		}
		// usemtl
		else if(cur_word == "usemtl") {
			if(join_mat_objects) {
				buffer >> val1;
				if(object_mats.count(val1) == 0) {
					cur_subobj = obj_names->size();
					object_mats[val1] = cur_subobj;
					(*obj_names)[cur_subobj] = val1;
					if(obj_mats != NULL) {
						(*obj_mats)[cur_subobj] = val1;
					}
				}
				else {
					// if join_mat_objects is specified, reuse to sub-object id, thus merging all data for one material
					cur_subobj = object_mats[val1];
				}
			}
			else {
				if(obj_mats != NULL) {
					(*obj_mats)[cur_subobj] = val1;
				}
			}
		}
		// mtllib
		else if(cur_word == "mtllib") {
			buffer >> val1;
			mtllib = val1;
		}
		// face / triangle
		else if(cur_word == "f") {
			if(cur_subobj < 0) {
				a2e_error("invalid obj-format - no sub-object specified!");
				return false;
			}
			
			buffer >> val1;
			buffer >> val2;
			buffer >> val3;
			buffer >> val4;
			quad_face = true;
			
			// since the .obj format allows mixed triangle and quad faces, we have to check this each time ...
			if(!is_number(val4)) {
				cur_word = val4;
				quad_face = false;
				set_word = true;
			}
			
			pair<string, string> i1 = get_face_indices(val1);
			pair<string, string> i2 = get_face_indices(val2);
			pair<string, string> i3 = get_face_indices(val3);
			
			(*indices)[cur_subobj].push_back(new s_index());
			(*indices)[cur_subobj].back()->indices[0] = string2uint(i1.first) - 1;
			(*indices)[cur_subobj].back()->indices[1] = string2uint(i2.first) - 1;
			(*indices)[cur_subobj].back()->indices[2] = string2uint(i3.first) - 1;
			(*tex_indices)[cur_subobj].push_back(new s_index());
			(*tex_indices)[cur_subobj].back()->indices[0] = string2uint(i1.second) - 1;
			(*tex_indices)[cur_subobj].back()->indices[1] = string2uint(i2.second) - 1;
			(*tex_indices)[cur_subobj].back()->indices[2] = string2uint(i3.second) - 1;
			
			// if we have quad faces, add another triangle
			if(quad_face) {
				pair<string, string> i4 = get_face_indices(val4);				
				(*indices)[cur_subobj].push_back(new s_index());
				(*indices)[cur_subobj].back()->indices[0] = string2uint(i1.first) - 1;
				(*indices)[cur_subobj].back()->indices[1] = string2uint(i3.first) - 1;
				(*indices)[cur_subobj].back()->indices[2] = string2uint(i4.first) - 1;
				(*tex_indices)[cur_subobj].push_back(new s_index());
				(*tex_indices)[cur_subobj].back()->indices[0] = string2uint(i1.second) - 1;
				(*tex_indices)[cur_subobj].back()->indices[1] = string2uint(i3.second) - 1;
				(*tex_indices)[cur_subobj].back()->indices[2] = string2uint(i4.second) - 1;
			}
		}
		// sub-object
		else if(cur_word == "g") {
			if(buffer >> val1 && val1[0] != '#') {
				if(!collision_obj && val1 == "collision") {
					a2e_error("old obj-format - no sub-object with the name \"collision\" allowed!");
					return false;
				}
				
				if(!join_mat_objects) {
					cur_subobj = obj_names->size();
					(*obj_names)[cur_subobj] = val1;
					if(obj_mats != NULL) {
						(*obj_mats)[cur_subobj] = "";
					}
				}
			}
		}
		
		// if set_word is set, don't get a new one (a word was already set)
		if(!set_word) {
			buffer >> cur_word;
			is_word = !buffer.fail();
		}
		else set_word = false;
	}
	
	if(tex_coords->empty()) {
		a2e_error("obj doesn't contain texture coordinates - using dummy coordinates!");
		tex_coords->push_back(new coord());
		tex_coords->back()->u = 0.0f;
		tex_coords->back()->v = 0.0f;
	}
	
	return true;
}

void create_mat_mapping() {
	if(!mat_mapping) return;
	if(mtllib == "") {
		a2e_error("-mat_mapping specified, but no mtllib is specified inside the .obj file!");
		return;
	}
	
	stringstream buffer;
	file_io f;
	if(!f.file_to_buffer(mtllib.c_str(), buffer)) {
		a2e_error("-mat_mapping specified, but couldn't open mtllib!");
		return;
	}
	
	// create mtl_name -> id mapping
	map<string, size_t> mat_mapping;
	try {
		string cur_word, val1;
		cur_word.reserve(256);
		val1.reserve(256);
		
		buffer >> cur_word;
		bool is_word = !buffer.fail();
		bool set_word = false;
		while(is_word && !buffer.eof()) {
			// newmtl
			if(cur_word == "newmtl") {
				buffer >> val1;
				size_t id = mat_mapping.size();
				mat_mapping[val1] = id;
			}
			// ignore everything else
			
			// if set_word is set, don't get a new one (a word was already set)
			if(!set_word) {
				buffer >> cur_word;
				is_word = !buffer.fail();
			}
			else set_word = false;
		}
	}
	catch(...) {
		a2e_error("error while reading mtl file!");
		return;
	}
	
	// create and write mapping
	string mapping_fname = string(a2m_filename) + ".mapping.txt";
	if(!f.open(mapping_fname.c_str(), file_io::OT_WRITE)) {
		return;
	}
	
	fstream& file = *f.get_filestream();
	file << "\t<material_mapping>" << endl;
	for(map<unsigned int, string>::const_iterator mat_iter = model_mat_names.begin(); mat_iter != model_mat_names.end(); mat_iter++) {
		if(mat_mapping.count(mat_iter->second) == 0) {
			a2e_error("material %s doesn't exist in .mtl file!", mat_iter->second);
			file << "\t\t<object material_id=\"0\" />" << endl;
			continue;
		}
		file << "\t\t<object material_id=\"" << mat_mapping[mat_iter->second] << "\" />" << endl;
	}
	file << "\t</material_mapping>" << endl;
	
	f.close();
}

struct s_cmp_coord {
	bool operator() (coord* c1, coord* c2) const {
		if(c1->u < c2->u) return true;
		else if(c1->u > c2->u) return false;
		else { // ==
			if(c1->v < c2->v) return true;
			else if(c1->v > c2->v) return false;
			else return false;
		}
	}
} cmp_coord;

bool equal_vertex(const float3* v1, const float3* v2) {
	const static float epsilon = 0.001f;
	if(((v1->x - epsilon) < v2->x) && (v2->x < (v1->x + epsilon))) {
		if(((v1->y - epsilon) < v2->y) && (v2->y < (v1->y + epsilon))) {
			if(((v1->z - epsilon) < v2->z) && (v2->z < (v1->z + epsilon))) {
				return true;
			}
			else return false;
		}
		else return false;
	}
	return false;
}

bool equal_coord(coord* c1, coord* c2) {
	const static float epsilon = 0.000001f;
	if(((c1->u - epsilon) < c2->u) && (c2->u < (c1->u + epsilon))) {
		if(((c1->v - epsilon) < c2->v) && (c2->v < (c1->v + epsilon))) {
			return true;
		}
		else return false;
	}
	return false;
}

int main(int argc, char *argv[]) {
	logger::init();
	
#ifdef WIN32
	start_time = GetTickCount();
#else
	gettimeofday(&start_time, NULL);
#endif
	
	file_io f;
	
	a2e_log("obj2a2m v%u.%u.%u - %s %s", OBJ2A2M_MAJOR_VERSION, OBJ2A2M_MINOR_VERSION, OBJ2A2M_REVISION_VERSION, OBJ2A2M_BUILT_DATE, OBJ2A2M_BUILT_TIME);
	
	string usage = "usage: obj2a2m [-rotate | -rotate_model | -rotate_collision] [-collision collision.obj] [-to_obj] [-join_mat_objects] [-mat_mapping] model.obj model.a2m";
	if(argc == 1) {
		a2e_error("no .obj and .a2m file specified!\n%s", usage.c_str());
		return 0;
	}
	else if(argc == 2) {
		a2e_error("no .obj or .a2m file specified!\n%s", usage.c_str());
		return 0;
	}
	
	unsigned int used_args = 0;
	for(int i = 0; i < argc; i++) {
		if(strcmp(argv[i], "-rotate") == 0) {
			rotate_obj = true;
			rotate_model = true;
			rotate_collision = true;
			used_args++;
		}
		else if(strcmp(argv[i], "-rotate_model") == 0) {
			rotate_model = true;
			used_args++;
		}
		else if(strcmp(argv[i], "-rotate_collision") == 0) {
			rotate_collision = true;
			used_args++;
		}
		else if(strcmp(argv[i], "-to_obj") == 0) {
			to_obj = true;
			used_args++;
		}
		else if(strcmp(argv[i], "-collision") == 0) {
			used_args++;
			i++;
			if(i < argc) {
				collision_filename = argv[i];
				collision_object = true;
				used_args++;
			}
		}
		else if(strcmp(argv[i], "-join_mat_objects") == 0) {
			join_mat_objects = true;
			used_args++;
		}
		else if(strcmp(argv[i], "-mat_mapping") == 0) {
			mat_mapping = true;
			used_args++;
		}
	}

	if(used_args + 3 > (unsigned int)argc) {
		a2e_error("too few arguments!\n%s", usage.c_str());
		return -1;
	}
	
	obj_filename = argv[argc-2];
	a2m_filename = argv[argc-1];

	a2e_debug("converting \"%s\" to \"%s\" ...", obj_filename, a2m_filename);
	
	// read and store obj data
	a2e_debug("loading obj ...");
	if(!load_obj_data(false, obj_filename, &model_vertices, &model_tex_coords, &model_indices, &model_tex_indices, &model_obj_names, &model_mat_names)) {
		return -1;
	}
	
	if(collision_object) {
		a2e_debug("loading collision obj ...");
		if(!load_obj_data(true, collision_filename, &collision_vertices, &collision_tex_coords, &collision_indices, &collision_tex_indices, &collision_obj_names, NULL)) {
			return -1;
		}
		
		if(collision_obj_names.size() > 1) {
			a2e_error("collision model contains too many sub-objects - only one sub-object allowed!");
			return -1;
		}
		else if(collision_obj_names.size() == 0) {
			a2e_error("collision model has no object data!");
			return -1;
		}
	}
	
	// reduce data
	/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
	/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
	// load data
	a2e_debug("reducing data ...");
	object_count = model_obj_names.size();
	sub_objects = new sub_object[object_count];
	multimap<unsigned int, pair<float3*, coord*>> data_map;
	bool coord_found = false;
	float3* cur_vertex;
	for(unsigned int i = 0; i < object_count; i++) {
		data_map.clear();
		for(unsigned int j = 0; j < model_indices[i].size(); j++) {
			sub_objects[i].faces.push_back(new face());
			
			// first index
			for(unsigned int k = 0; k < 3; k++) {
				if(data_map.count(model_indices[i][j]->indices[k]) == 0) {
					// add vertex and texture coordinate to container
					sub_objects[i].vertices.push_back(new float3(model_vertices[model_indices[i][j]->indices[k]]));
					sub_objects[i].faces.back()->vertices[k] = sub_objects[i].vertices.back();
					sub_objects[i].coords.push_back(new coord());
					*sub_objects[i].coords.back() = *model_tex_coords[model_tex_indices[i][j]->indices[k]];
					sub_objects[i].faces.back()->coords[k] = sub_objects[i].coords.back();
					data_map.insert(pair<unsigned int, pair<float3*, coord*>>(model_indices[i][j]->indices[k], pair<float3*, coord*>(sub_objects[i].vertices.back(), sub_objects[i].coords.back())));
				}
				else {
					// vertex already exists, only add texture coordinate (if it doesn't exist already)
					cur_vertex = data_map.equal_range(model_indices[i][j]->indices[k]).first->second.first; // sick, but working ...
					sub_objects[i].faces.back()->vertices[k] = cur_vertex;
					coord_found = false;
					for(multimap<unsigned int, pair<float3*, coord*>>::iterator diter = data_map.equal_range(model_indices[i][j]->indices[k]).first;
						diter != data_map.equal_range(model_indices[i][j]->indices[k]).second; diter++) {
						if(equal_coord(diter->second.second, model_tex_coords[model_tex_indices[i][j]->indices[k]])) {
							coord_found = true;
							sub_objects[i].faces.back()->coords[k] = diter->second.second;
						}
					}
					// if coord doesn't exist already, add new one
					if(!coord_found) {
						sub_objects[i].coords.push_back(new coord());
						*sub_objects[i].coords.back() = *model_tex_coords[model_tex_indices[i][j]->indices[k]];
						sub_objects[i].faces.back()->coords[k] = sub_objects[i].coords.back();
						data_map.insert(pair<unsigned int, pair<float3*, coord*>>(model_indices[i][j]->indices[k], pair<float3*, coord*>(cur_vertex, sub_objects[i].coords.back())));
					}
				}
			}
		}
	}
	
	// sort vertices in each sub-object
	a2e_debug("sorting vertices ...");
	for(unsigned int i = 0; i < object_count; i++) {
		sort(sub_objects[i].vertices.begin(), sub_objects[i].vertices.end(), cmp_vertex);
	}
	
	// look for equal vertices and remove duplicates
	a2e_debug("removing duplicate vertices ...");
	float3* equal_vert;
	float3* cur_vert;
	map<float3*, float3*> replace_vertices;
	for(unsigned int i = 0; i < object_count; i++) {
		unsigned int j = 0;
		unsigned int vertices_size = sub_objects[i].vertices.size() - 1; // -1, b/c a vertex is always compared with the next one (-> not needed by the last)
		while(j < vertices_size) {
			// if the current and next vertex are equal ...
			if(equal_vertex(sub_objects[i].vertices[j], sub_objects[i].vertices[j+1])) {
				cur_vert = sub_objects[i].vertices[j];
				equal_vert = sub_objects[i].vertices[j+1];
				replace_vertices[equal_vert] = cur_vert;
				
				// delete next vertex
				sub_objects[i].vertices.erase(sub_objects[i].vertices.begin() + j + 1);
				vertices_size--; // size was decreased by one ...
				
				// don't increase j here, b/c the next vertex might be the same again ...
			}
			else j++;
		}
		
		// replace vertex pointers
		a2e_debug("in sub-object #%u ...", i);
		unsigned int fcnt = 0;
		for(vector<face*>::iterator fiter = sub_objects[i].faces.begin(); fiter != sub_objects[i].faces.end(); fiter++) {
			for(map<float3*, float3*>::iterator rv_iter = replace_vertices.begin(); rv_iter != replace_vertices.end(); rv_iter++) {
				if((*fiter)->vertices[0] == rv_iter->first) (*fiter)->vertices[0] = rv_iter->second;
				if((*fiter)->vertices[1] == rv_iter->first) (*fiter)->vertices[1] = rv_iter->second;
				if((*fiter)->vertices[2] == rv_iter->first) (*fiter)->vertices[2] = rv_iter->second;
			}
			fcnt++;
			
			if((fcnt % 10000) == 0) cout << (fcnt / 10000) << "0k ";
		}
		if(fcnt > 10000) cout << endl;
	}
	
	// make indices
	a2e_debug("creating new indices ...");
	map<float3*, unsigned int> vertex_indices;
	map<coord*, unsigned int> tex_indices;
	unsigned int v_index = 0;
	unsigned int tc_index = 0;
	for(unsigned int i = 0; i < object_count; i++) {
		for(unsigned int j = 0; j < sub_objects[i].vertices.size(); j++) {
			vertex_indices[sub_objects[i].vertices[j]] = v_index;
			v_index++;
		}
		for(unsigned int j = 0; j < sub_objects[i].coords.size(); j++) {
			tex_indices[sub_objects[i].coords[j]] = tc_index;
			tc_index++;
		}
	}
	
	for(unsigned int i = 0; i < object_count; i++) {
		for(vector<face*>::iterator fiter = sub_objects[i].faces.begin(); fiter != sub_objects[i].faces.end(); fiter++) {
			(*fiter)->vertex_indices.indices[0] = vertex_indices[(*fiter)->vertices[0]];
			(*fiter)->vertex_indices.indices[1] = vertex_indices[(*fiter)->vertices[1]];
			(*fiter)->vertex_indices.indices[2] = vertex_indices[(*fiter)->vertices[2]];
			(*fiter)->tex_indices.indices[0] = tex_indices[(*fiter)->coords[0]];
			(*fiter)->tex_indices.indices[1] = tex_indices[(*fiter)->coords[1]];
			(*fiter)->tex_indices.indices[2] = tex_indices[(*fiter)->coords[2]];
		}
	}
	
	/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
	/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
	
	// debug output to new .obj
	if(to_obj) {
		string debug_obj = string(a2m_filename).substr(0, strlen(a2m_filename)-3) + "obj";
		a2e_debug("saving to %s ...", debug_obj.c_str());
		
		if(!f.open(debug_obj.c_str(), file_io::OT_WRITE_BINARY)) {
			a2e_error("couldn't open/write obj file \"%s\"!", debug_obj.c_str());
			return -1;
		}
		
		fstream* fs = f.get_filestream();
		
		for(unsigned int i = 0; i < object_count; i++) {
			*fs << "# vc " << i << ": " << sub_objects[i].vertices.size() << endl;
			*fs << "# tc " << i << ": " << sub_objects[i].coords.size() << endl;
			*fs << "# fc " << i << ": " << sub_objects[i].faces.size() << endl;
		}
		
		for(unsigned int i = 0; i < object_count; i++) {
			for(unsigned int j = 0; j < sub_objects[i].vertices.size(); j++) {
				if(!rotate_model) {
					*fs << "v " << sub_objects[i].vertices[j]->x << " " << sub_objects[i].vertices[j]->y << " " << sub_objects[i].vertices[j]->z << endl;
				}
				else {
					*fs << "v " << sub_objects[i].vertices[j]->x << " " << sub_objects[i].vertices[j]->z << " " << -sub_objects[i].vertices[j]->y << endl;
				}
			}
		}
		for(unsigned int i = 0; i < object_count; i++) {
			for(unsigned int j = 0; j < sub_objects[i].coords.size(); j++) {
				*fs << "vt " << sub_objects[i].coords[j]->u << " " << sub_objects[i].coords[j]->v << endl;
			}
		}
		
		for(unsigned int i = 0; i < object_count; i++) {
			*fs << "g " << model_obj_names[i] << endl;
			*fs << "usemtl " << model_mat_names[i] << endl;
			
			for(vector<face*>::iterator fiter = sub_objects[i].faces.begin(); fiter != sub_objects[i].faces.end(); fiter++) {
				*fs << "f " << ((*fiter)->vertex_indices.indices[0]+1) << "/" << ((*fiter)->tex_indices.indices[0]+1) << " " << ((*fiter)->vertex_indices.indices[1]+1) << "/" << ((*fiter)->tex_indices.indices[1]+1)
				<< " " << ((*fiter)->vertex_indices.indices[2]+1) << "/" << ((*fiter)->tex_indices.indices[2]+1) << endl;
			}
			*fs << endl;
		}
		*fs << endl;
		
		f.close();
	}
	
	/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
	/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
	
	unsigned int total_vertex_count = 0;
	unsigned int total_coord_count = 0;
	for(unsigned int i = 0; i < object_count; i++) {
		total_vertex_count += sub_objects[i].vertices.size();
		total_coord_count += sub_objects[i].coords.size();
	}
	
	// convert obj data to a2m, save a2m
	if(!to_obj) {
		a2e_debug("saving a2m ...");
		if(!f.open(a2m_filename, file_io::OT_WRITE_BINARY)) {
			a2e_error("couldn't open/write a2m file \"%s\"!", a2m_filename);
			return -1;
		}
		
		f.write_block("A2EMODEL", 8);
		f.write_uint(A2M_VERSION);
		f.write_char(collision_object ? 0x02 : 0x00);
		
		f.write_uint(total_vertex_count);
		f.write_uint(total_coord_count);
		
		for(unsigned int i = 0; i < object_count; i++) {
			for(unsigned int j = 0; j < sub_objects[i].vertices.size(); j++) {
				if(!rotate_model) {
					f.write_float(sub_objects[i].vertices[j]->x);
					f.write_float(sub_objects[i].vertices[j]->y);
					f.write_float(sub_objects[i].vertices[j]->z);
				}
				else {
					f.write_float(sub_objects[i].vertices[j]->x);
					f.write_float(sub_objects[i].vertices[j]->z);
					f.write_float(-sub_objects[i].vertices[j]->y);
				}
			}
		}
		for(unsigned int i = 0; i < object_count; i++) {
			for(unsigned int j = 0; j < sub_objects[i].coords.size(); j++) {
				f.write_float(sub_objects[i].coords[j]->u);
				f.write_float(sub_objects[i].coords[j]->v);
			}
		}
		
		f.write_uint(object_count);
		for(map<unsigned int, string>::iterator oiter = model_obj_names.begin(); oiter != model_obj_names.end(); oiter++) {
			f.write_terminated_block(oiter->second, 0xFF);
		}
		
		for(unsigned int i = 0; i < object_count; i++) {
			f.write_uint(sub_objects[i].faces.size());
			for(vector<face*>::iterator fiter = sub_objects[i].faces.begin(); fiter != sub_objects[i].faces.end(); fiter++) {
				f.write_uint((*fiter)->vertex_indices.indices[0]);
				f.write_uint((*fiter)->vertex_indices.indices[1]);
				f.write_uint((*fiter)->vertex_indices.indices[2]);
			}
			for(vector<face*>::iterator fiter = sub_objects[i].faces.begin(); fiter != sub_objects[i].faces.end(); fiter++) {
				f.write_uint((*fiter)->tex_indices.indices[0]);
				f.write_uint((*fiter)->tex_indices.indices[1]);
				f.write_uint((*fiter)->tex_indices.indices[2]);
			}
		}
		
		if(collision_object) {
			f.write_uint(collision_vertices.size());
			for(vector<float3*>::iterator viter = collision_vertices.begin(); viter != collision_vertices.end(); viter++) {
				if(!rotate_collision) {
					f.write_float((*viter)->x);
					f.write_float((*viter)->y);
					f.write_float((*viter)->z);
				}
				else {
					f.write_float((*viter)->x);
					f.write_float((*viter)->z);
					f.write_float(-(*viter)->y);
				}
			}
			
			f.write_uint(collision_indices[0].size());
			for(vector<s_index*>::iterator iiter = model_indices[0].begin(); iiter != model_indices[0].end(); iiter++) {
				f.write_uint((*iiter)->indices[0]);
				f.write_uint((*iiter)->indices[1]);
				f.write_uint((*iiter)->indices[2]);
			}
		}
		
		f.close();
	}
	
	//
	create_mat_mapping();

	// done!
	int reduced_vertices = model_vertices.size() - total_vertex_count;
	int reduced_tex_coords = model_tex_coords.size() - total_coord_count;
	if(reduced_vertices > 0) a2e_debug("reduced model by %u vertices!", reduced_vertices);
	if(reduced_tex_coords > 0) a2e_debug("reduced model by %u texture coordinates!", reduced_tex_coords);
	a2e_debug("successfully converted \"%s\" to \"%s\" (%u sub-object%s, %u vertices, %u texture coordinates)!",
			 obj_filename, a2m_filename, model_obj_names.size(), (model_obj_names.size() == 1 ? "" : "s"), total_vertex_count, total_coord_count);

#ifdef WIN32
	stop_time = GetTickCount();
	a2e_debug("time needed: %ds / %ums", (stop_time - start_time)/1000, stop_time - start_time);
#else
	gettimeofday(&stop_time, NULL);
	a2e_debug("time needed: %ds / %ums", (stop_time.tv_sec - start_time.tv_sec), (stop_time.tv_sec - start_time.tv_sec) * 1000 + (stop_time.tv_usec - start_time.tv_usec) / 1000);
#endif

	logger::destroy();

	return 0;
}
