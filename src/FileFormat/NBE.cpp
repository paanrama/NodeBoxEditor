#include <fstream>
#include <stdlib.h>
#include "NBE.hpp"
#include "../util/string.hpp"
#include "../util/filesys.hpp"
#include "../util/SimpleFileCombiner.hpp"

Project *NBEFileFormat::read(const std::string &filename)
{
	std::string tmpdir = getTmpDirectory(state->settings->getBool("installed"));
	std::cerr << "Temp directory is " << tmpdir << std::endl;
	CreateDir(tmpdir);
	Project *project = new Project();
	SimpleFileCombiner fc;
	std::list<std::string> files = fc.read(filename.c_str(), tmpdir);
	for (std::list<std::string>::const_iterator it = files.begin();
			it != files.end();
			++it) {
		std::string name = *it;
		std::cerr << "Adding " << name << std::endl;
		project->media.add(name.c_str(), state->device->getVideoDriver()->createImageFromFile((tmpdir + name).c_str()));
	}
	if (!readProjectFile(project, tmpdir + "project.txt")) {
		delete project;
		return NULL;
	}
	return project;
}

bool NBEFileFormat::write(Project *project, const std::string &filename)
{
	std::string tmpdir = getTmpDirectory(state->settings->getBool("installed"));
	std::cerr << "Temp directory is " << tmpdir << std::endl;
	CreateDir(tmpdir);
	SimpleFileCombiner fc;
	Media *media = &project->media;
	std::map<std::string, Media::Image*>& images = media->getList();
	for (std::map<std::string, Media::Image*>::const_iterator it = images.begin();
			it != images.end();
			++it) {
		Media::Image *image = it->second;	
		std::cerr << "Adding image " << image->name.c_str() << std::endl;
		if (!image->get())
			std::cerr << "Image->get() is NULL!" << std::endl;	
		state->device->getVideoDriver()->writeImageToFile(image->get(), "tmp/two.png");
		fc.add((tmpdir + image->name).c_str(), image->name);
		std::cerr << "Added" << std::endl;
	}
	if (writeProjectFile(project, tmpdir + "project.txt")) {
		fc.add((tmpdir + "project.txt").c_str(), "project.txt");
		std::cerr << "writing..." << std::endl;
		return fc.write(filename);
	}
	return true;
}

bool NBEFileFormat::readProjectFile(Project *project, const std::string & filename)
{
	// Open file
	std::string line;
	std::ifstream file(filename.c_str());
	if (!file) {
		return false;
	}

	// Read parser header
	std::getline(file, line);
	if (line != "MINETEST NODEBOX EDITOR") {
		return false;
	}
	std::getline(file, line);
	if (line != "PARSER 1" && line != "PARSER 2") {
		return false;
	}

	// Parse file
	stage = READ_STAGE_ROOT;
	while (std::getline(file, line)) {
		parseLine(project, line);
	}
	file.close();
	return true;
}

bool NBEFileFormat::writeProjectFile(Project *project, const std::string &filename)
{
	std::ofstream file(filename.c_str());
	if (!file) {
		return false;
	}
	file << "MINETEST NODEBOX EDITOR\n";
	file << "PARSER 1\n";
	file << "NAME " << project->name << "\n\n";

	std::list<Node*> & nodes = project->nodes;
	unsigned int i = 0;
	for (std::list<Node*>::const_iterator it = nodes.begin();
			it != nodes.end();
			++it, ++i) {
		Node* node = *it;
		file << "NODE ";
		if (node->name == "") {
			file << "Node" << i;
		} else {
			file << node->name;
		}
		file << "\n";
		vector3di pos = node->position;
		file << "POSITION " << pos.X << ' ' << pos.Y << ' ' << pos.Z << '\n';

		for (std::vector<NodeBox*>::const_iterator it = node->boxes.begin();
				it != node->boxes.end();
				++it) {
			NodeBox* box = *it;
			file << "NODEBOX " << box->name << ' ';
			file << box->one.X << ' ' << box->one.Y << ' ' << box->one.Z << ' ';
			file << box->two.X << ' ' << box->two.Y << ' ' << box->two.Z << '\n';
		}

		file << "END NODE\n\n";
	}

	file.close();

	return true;
}

void NBEFileFormat::parseLine(Project * project, std::string & line)
{
	line = trim(line);

	if (line.empty()) {
		return;
	}

	std::string lower = str_to_lower(line);

	if (stage == READ_STAGE_ROOT) {
		if (lower.find("name ") == 0) {
			project->name = trim(line.substr(4));
		} else if (lower.find("node ") == 0) {
			stage = READ_STAGE_NODE;
			node = new Node(state->device, state, project->GetNodeCount());
			node->name = trim(line.substr(4));
		}
	} else if (stage == READ_STAGE_NODE) {
		if (lower.find("position ") == 0) {
			// TODO: Parse position
		} else if (lower.find("nodebox ") == 0){
			std::string n = trim(line.substr(7));
			std::string s[7];
			for (unsigned int i = 0; n != ""; i++){
				size_t nid = n.find(" ");

				if (nid == std::string::npos){
					nid = n.size();
				}
				if (i >= 7) {
					// Too many arguments to nodebox tag
					break;
				}
				s[i] = trim(n.substr(0, nid));
				n = trim(n.substr(nid));
			}
			node->addNodeBox();
			node->GetCurrentNodeBox()->name = s[0];
			node->GetCurrentNodeBox()->one = vector3df(
				atof(s[1].c_str()),
				atof(s[2].c_str()),
				atof(s[3].c_str())
			);
			node->GetCurrentNodeBox()->two = vector3df(
				atof(s[4].c_str()),
				atof(s[5].c_str()),
				atof(s[6].c_str())
			);
			node->remesh();
		} else if (lower.find("end node") == 0){
			project->AddNode(node);
			node = NULL;
			stage = READ_STAGE_ROOT;
		}
	}
}


