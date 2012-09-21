/*
 *  kernelcacher
 *  Copyright (C) 2012 Florian Ziesche
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

#include "kernelcacher.h"
#include <cl/cudacl_translator.h>
#include <threading/task.h>
#include "zlib.h"

/*!
 * \mainpage
 *
 * \author flo
 *
 * \date September 2012
 *
 * Albion 2 Engine Tool - Kernel Cacher (for CUDACL)
 */

static string kernel_path = "";
static string cache_path = "";
atomic<unsigned int> task_counter { 0 };

enum class CC_TARGET : unsigned int {
	SM_10,
	SM_11,
	SM_12,
	SM_13,
	SM_20,
	SM_21,
	SM_30,
	SM_35,
};
static const vector<pair<CC_TARGET, const char*>> cc_targets {
	{
		{ CC_TARGET::SM_10, "10" },
		{ CC_TARGET::SM_11, "11" },
		{ CC_TARGET::SM_12, "12" },
		{ CC_TARGET::SM_13, "13" },
		
		{ CC_TARGET::SM_20, "20" },
		{ CC_TARGET::SM_21, "21" },
		
		{ CC_TARGET::SM_30, "30" },
		{ CC_TARGET::SM_35, "35" },
	}
};

void kernel_to_ptx(const string& identifier, const string& file_name, const string& func_name,
				   std::function<string(const CC_TARGET&)> additional_options_fnc) {
	a2e_debug("compiling \"%s\" kernel!", identifier);
	
	//
	stringstream buffer(stringstream::in | stringstream::out);
	if(!file_io::file_to_buffer(file_name, buffer)) {
		a2e_error("failed to read kernel source!");
		return;
	}
	const string src(buffer.str());
	
	// nvcc compile
	for(const auto& target : cc_targets) {
		const string cc_target_str = target.second;
		
		// generate options
		string options = "-I " + kernel_path;
		string nvcc_log = "";
		const string additional_options(additional_options_fnc(target.first));
		if(!additional_options.empty()) {
			// convert all -DDEFINEs to -D DEFINE
			options += " " + core::find_and_replace(additional_options, "-D", "-D ");
		}
		
		// add kernel
		const string tmp_name = "/tmp/cudacl_tmp_"+identifier+"_"+cc_target_str+"_"+size_t2string(SDL_GetPerformanceCounter());
		vector<cudacl_kernel_info> kernels_info;
		string cuda_source = "";
		cudacl_translate(tmp_name, src.c_str(), options, cuda_source, kernels_info);
		
		// create tmp cu file
		fstream cu_file(tmp_name+".cu", fstream::out);
		cu_file << cuda_source << endl;
		cu_file.close();
		
		//
		string build_cmd = "/usr/local/cuda/bin/nvcc --ptx --machine 64 -arch sm_" + cc_target_str + " -O3";
		build_cmd += " " + options;
		
		//
		build_cmd += " -D NVIDIA";
		build_cmd += " -D GPU";
		build_cmd += " -D PLATFORM_NVIDIA";
		build_cmd += " -o "+tmp_name+".ptx";
		build_cmd += " "+tmp_name+".cu";
		//build_cmd += " 2>&1";
		core::system(build_cmd.c_str(), nvcc_log);
		
		// read ptx
		stringstream ptx_buffer(stringstream::in | stringstream::out);
		if(!file_io::file_to_buffer(tmp_name+".ptx", ptx_buffer)) {
			a2e_error("ptx file \"%s\" doesn't exist!", tmp_name+".ptx");
			return;
		}
		
		// write to cache
		// ptx:
		file_io ptx_out(cache_path+identifier+"_"+cc_target_str+".ptx", file_io::OPEN_TYPE::WRITE);
		if(!ptx_out.is_open()) {
			a2e_error("couldn't create ptx cache file for %s!", identifier);
			return;
		}
		const string ptx_data(ptx_buffer.str());
		ptx_out.write_block(ptx_data.c_str(), ptx_data.size());
		ptx_out.close();
		
		// kernel info:
		file_io info_out(cache_path+identifier+"_info_"+cc_target_str+".txt", file_io::OPEN_TYPE::WRITE);
		if(!info_out.is_open()) {
			a2e_error("couldn't create kernel info cache file for %s!", identifier);
			return;
		}
		auto& info_stream = *info_out.get_filestream();
		bool found = false;
		for(const auto& info : kernels_info) {
			if(info.name == func_name) {
				found = true;
				info_stream << info.name << " " << info.parameters.size();
				for(const auto& param : info.parameters) {
					info_stream << " " << get<0>(param);
					info_stream << " " << (unsigned int)get<1>(param);
					info_stream << " " << (unsigned int)get<2>(param);
					info_stream << " " << (unsigned int)get<3>(param);
				}
				break;
			}
		}
		info_out.close();
		if(!found) a2e_error("kernel function \"%s\" does not exist in source file!", func_name);
	}
	task_counter--;
}

int main(int argc, char *argv[]) {
	logger::init();
	
	a2e_log("kernelcacher v%u.%u.%u - %s %s", KERNELCACHER_MAJOR_VERSION, KERNELCACHER_MINOR_VERSION, KERNELCACHER_REVISION_VERSION, KERNELCACHER_BUILT_DATE, KERNELCACHER_BUILT_TIME);
	
	string usage = "usage: kernelcacher /path/to/data/kernels";
	if(argc == 1) {
		a2e_error("no kernel path specified!\n%s", usage.c_str());
		return 0;
	}
	kernel_path = argv[1];
	if(kernel_path.back() != '/') kernel_path.push_back('/');
	cache_path = kernel_path.substr(0, kernel_path.rfind('/', kernel_path.length()-2)) + "/cache/";
	a2e_debug("caching kernels from \"%s\" to \"%s\" ...", kernel_path, cache_path);
	
	// compile kernels
	const string lsl_sm_1x_str = " -DLOCAL_SIZE_LIMIT=512";
	const string lsl_sm_20p_str = " -DLOCAL_SIZE_LIMIT=1024";
	
	vector<tuple<string, string, string, std::function<string(const CC_TARGET&)>>> internal_kernels {
		{
			make_tuple("PARTICLE_INIT", "particle_spawn.cl", "particle_init",
					   [](const CC_TARGET&) { return " -DA2E_PARTICLE_INIT"; }),
			make_tuple("PARTICLE_RESPAWN", "particle_spawn.cl", "particle_respawn",
					   [](const CC_TARGET&) { return ""; }),
			make_tuple("PARTICLE_COMPUTE", "particle_compute.cl", "particle_compute",
					   [](const CC_TARGET&) { return ""; }),
			make_tuple("PARTICLE_SORT_LOCAL", "particle_sort.cl", "bitonicSortLocal",
					   [&](const CC_TARGET& cc_target) { return (cc_target <= CC_TARGET::SM_13 ? lsl_sm_1x_str : lsl_sm_20p_str); }),
			make_tuple("PARTICLE_SORT_MERGE_GLOBAL", "particle_sort.cl", "bitonicMergeGlobal",
					   [&](const CC_TARGET& cc_target) { return (cc_target <= CC_TARGET::SM_13 ? lsl_sm_1x_str : lsl_sm_20p_str); }),
			make_tuple("PARTICLE_SORT_MERGE_LOCAL", "particle_sort.cl", "bitonicMergeLocal",
					   [&](const CC_TARGET& cc_target) { return (cc_target <= CC_TARGET::SM_13 ? lsl_sm_1x_str : lsl_sm_20p_str); }),
			make_tuple("PARTICLE_COMPUTE_DISTANCES", "particle_sort.cl", "compute_distances",
					   [&](const CC_TARGET& cc_target) { return (cc_target <= CC_TARGET::SM_13 ? lsl_sm_1x_str : lsl_sm_20p_str); }),
		}
	};
	task_counter = internal_kernels.size() + 1;
	
	for(const auto& int_kernel : internal_kernels) {
		task::spawn([=]() {
			kernel_to_ptx(get<0>(int_kernel),
						  kernel_path+get<1>(int_kernel),
						  get<2>(int_kernel),
						  get<3>(int_kernel));
		});
	}
	
	//
	task::spawn([=]() {
		file_io crc_file(cache_path+"CACHECRC", file_io::OPEN_TYPE::WRITE);
		if(!crc_file.is_open()) {
			a2e_error("couldn't create crc file!");
			return;
		}
		auto& crc_fstream = *crc_file.get_filestream();
		
		const auto kernel_files = core::get_file_list(kernel_path);
		for(const auto& kfile : kernel_files) {
			if(kfile.first[0] == '.') continue;
			stringstream buffer(stringstream::in | stringstream::out);
			if(!file_io::file_to_buffer(kernel_path+kfile.first, buffer)) {
				a2e_error("failed to read kernel source!");
				return;
			}
			const string src(buffer.str());
			const unsigned int crc = crc32(crc32(0L, Z_NULL, 0), (const Bytef*)src.c_str(), (uInt)src.size());
			crc_fstream << kfile.first << " " << hex << crc << dec << endl;
		}
		crc_file.close();
		task_counter--;
	});
	
	//
	while(task_counter != 0) {
		SDL_Delay(100);
	}

	// done!
	logger::destroy();
	return 0;
}
