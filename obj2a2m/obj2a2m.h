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

#ifndef __OBJ2A2M_H__
#define __OBJ2A2M_H__

#define A2M_VERSION 2

#define OBJ2A2M_MAJOR_VERSION 0
#define OBJ2A2M_MINOR_VERSION 3
#define OBJ2A2M_REVISION_VERSION 4
#define OBJ2A2M_BUILT_TIME __TIME__
#define OBJ2A2M_BUILT_DATE __DATE__

#include <a2e.h>
#include <ctime>
#ifndef WIN32
#include <sys/time.h>
#endif

#ifdef WIN32
DWORD start_time;
DWORD stop_time;
#else
timeval start_time;
timeval stop_time;
#endif

struct s_cmp_vertex {
	bool operator() (float3* v1, float3* v2) const {
		if(v1->x < v2->x) return true;
		else if(v1->x > v2->x) return false;
		else { // ==
			if(v1->y < v2->y) return true;
			else if(v1->y > v2->y) return false;
			else { // ==
				if(v1->z < v2->z) return true;
				else if(v1->z > v2->z) return false;
				else return false;
			}
		}
	}
} cmp_vertex;

struct s_cmp_vtc_pair {
	bool operator() (const pair<float3*, coord*>& p1, const pair<float3*, coord*>& p2) const {
		if(p1.first == p2.first) {
			if(p1.second->u < p2.second->u) return true;
			else if(p1.second->u > p2.second->u) return false;
			else { // ==
				if(p1.second->v < p2.second->v) return true;
				else if(p1.second->v > p2.second->v) return false;
				else return true;
			}
		}
		else {
			return cmp_vertex(p1.first, p2.first);
		}
	}
} cmp_vtc_pair;

struct s_index {
	unsigned int indices[3];
};

struct face {
	float3* vertices[3];
	coord* coords[3];
	
	s_index vertex_indices;
	s_index tex_indices;
};

struct sub_object {
	vector<face*> faces;
	vector<float3*> vertices;
	vector<coord*> coords;
	multiset<pair<float3*, coord*>, s_cmp_vtc_pair> data;
	string mat_name;
};
sub_object* sub_objects;


bool load_obj_data(bool collision_obj, const char* filename, vector<float3*>* vertices, vector<coord*>* tex_coords, map<unsigned int, vector<s_index*>>* indices, map<unsigned int, vector<s_index*>>* tex_indices, map<unsigned int, string>* obj_names, map<unsigned int, string>* obj_mats);


bool rotate_obj = false;
bool rotate_model = false;
bool rotate_collision = false;
bool collision_object = false;
bool to_obj = false;
bool join_mat_objects = false;
bool mat_mapping = false;

char* obj_filename;
char* collision_filename;
char* a2m_filename;

unsigned int object_count = 0;
string mtllib = "";

vector<float3*> model_vertices;
vector<coord*> model_tex_coords;
map<unsigned int, vector<s_index*>> model_indices;
map<unsigned int, vector<s_index*>> model_tex_indices;
map<unsigned int, string> model_obj_names;
map<unsigned int, string> model_mat_names;

vector<float3*> collision_vertices;
vector<coord*> collision_tex_coords;
map<unsigned int, vector<s_index*>> collision_indices;
map<unsigned int, vector<s_index*>> collision_tex_indices;
map<unsigned int, string> collision_obj_names;

#endif
