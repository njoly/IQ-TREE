/***************************************************************************
 *   Copyright (C) 2009 by BUI Quang Minh   *
 *   minh.bui@univie.ac.at   *
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 *   This program is distributed in the hope that it will be useful,       *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
 *   GNU General Public License for more details.                          *
 *                                                                         *
 *   You should have received a copy of the GNU General Public License     *
 *   along with this program; if not, write to the                         *
 *   Free Software Foundation, Inc.,                                       *
 *   59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.             *
 ***************************************************************************/
#include "phylosupertree.h"
#include "superalignment.h"
#include "superalignmentpairwise.h"
#include "msetsblock.h"
#include "myreader.h"
#include "phylotesting.h"

PhyloSuperTree::PhyloSuperTree()
 : IQTree()
{
	totalNNIs = evalNNIs = 0;
    rescale_codon_brlen = false;
	// Initialize the counter for evaluated NNIs on subtrees. FOR THIS CASE IT WON'T BE initialized.
}

PhyloSuperTree::PhyloSuperTree(SuperAlignment *alignment, PhyloSuperTree *super_tree) :  IQTree(alignment) {
	totalNNIs = evalNNIs = 0;
    rescale_codon_brlen = super_tree->rescale_codon_brlen;
	part_info = super_tree->part_info;
	for (vector<Alignment*>::iterator it = alignment->partitions.begin(); it != alignment->partitions.end(); it++) {
		PhyloTree *tree = new PhyloTree((*it));
		push_back(tree);
	}
	// Initialize the counter for evaluated NNIs on subtrees
	int part = 0;
	for (iterator it = begin(); it != end(); it++, part++) {
		part_info[part].evalNNIs = 0.0;
	}

	aln = alignment;
}

void PhyloSuperTree::setCheckpoint(Checkpoint *checkpoint) {
	IQTree::setCheckpoint(checkpoint);
	for (iterator it = begin(); it != end(); it++)
		(*it)->setCheckpoint(checkpoint);
}

void PhyloSuperTree::saveCheckpoint() {
//    checkpoint->startStruct("PhyloSuperTree");
//    int part = 0;
//    for (iterator it = begin(); it != end(); it++, part++) {
//    	string key = part_info[part].name + ".tree";
//    	checkpoint->put(key, (*it)->getTreeString());
//    }
//    checkpoint->endStruct();
    IQTree::saveCheckpoint();
}

void PhyloSuperTree::restoreCheckpoint() {
    IQTree::restoreCheckpoint();
    
    // first get the newick string of super tree
//    checkpoint->startStruct("PhyloTree");
//    string newick;
//    CKP_RESTORE(newick);
//    checkpoint->endStruct();
//
//    if (newick.empty()) return;
//    
//    // now get partition tree strings
//    checkpoint->startStruct("PhyloSuperTree");
//    int part = 0;
//    for (iterator it = begin(); it != end(); it++, part++) {
//    	string key = part_info[part].name + ".tree";
//    	string part_tree;
//    	if (!checkpoint->get(key, part_tree))
//    		outError("No tree for partition " + part_info[part].name + " found from checkpoint");
//    	newick += part_tree;
//    }
//
//    checkpoint->endStruct();
//
//    readTreeString(newick);

}

void PhyloSuperTree::readPartition(Params &params) {
	try {
		ifstream in;
		in.exceptions(ios::failbit | ios::badbit);
		in.open(params.partition_file);
		in.exceptions(ios::badbit);
		Params origin_params = params;
		PartitionInfo info;

		while (!in.eof()) {
			getline(in, info.name, ',');
			if (in.eof()) break;
			getline(in, info.model_name, ',');
			if (info.model_name == "") info.model_name = params.model_name;
			getline(in, info.aln_file, ',');
			if (info.aln_file == "" && params.aln_file) info.aln_file = params.aln_file;
			getline(in, info.sequence_type, ',');
			if (info.sequence_type=="" && params.sequence_type) info.sequence_type = params.sequence_type;
			getline(in, info.position_spec);
            trimString(info.sequence_type);
			cout << endl << "Reading partition " << info.name << " (model=" << info.model_name << ", aln=" <<
					info.aln_file << ", seq=" << info.sequence_type << ", pos=" << ((info.position_spec.length() >= 20) ? info.position_spec.substr(0,20)+"..." : info.position_spec) << ") ..." << endl;

			//info.mem_ptnlh = NULL;
			info.nniMoves[0].ptnlh = NULL;
			info.nniMoves[1].ptnlh = NULL;
			info.cur_ptnlh = NULL;
			part_info.push_back(info);
			Alignment *part_aln = new Alignment((char*)info.aln_file.c_str(), (char*)info.sequence_type.c_str(), params.intype);
			if (!info.position_spec.empty()) {
				Alignment *new_aln = new Alignment();
				new_aln->extractSites(part_aln, info.position_spec.c_str());
				delete part_aln;
				part_aln = new_aln;
			}
			PhyloTree *tree = new PhyloTree(part_aln);
			push_back(tree);
			params = origin_params;
		}

		in.clear();
		// set the failbit again
		in.exceptions(ios::failbit | ios::badbit);
		in.close();
	} catch(ios::failure) {
		outError(ERR_READ_INPUT);
	} catch (string str) {
		outError(str);
	}


}

void PhyloSuperTree::readPartitionRaxml(Params &params) {
	try {
		ifstream in;
		in.exceptions(ios::failbit | ios::badbit);
		in.open(params.partition_file);
		in.exceptions(ios::badbit);
		PartitionInfo info;
        Alignment *input_aln = NULL;
        if (!params.aln_file)
            outError("Please supply an alignment with -s option");
            
        input_aln = new Alignment(params.aln_file, params.sequence_type, params.intype);

        cout << endl << "Partition file is not in NEXUS format, assuming RAxML-style partition file..." << endl;

        size_t pos = params.model_name.find_first_of("+*");
        string rate_type = "";
        if (pos != string::npos) rate_type = params.model_name.substr(pos);

		while (!in.eof()) {
			getline(in, info.model_name, ',');
			if (in.eof()) break;
            trimString(info.model_name);
//            std::transform(info.model_name.begin(), info.model_name.end(), info.model_name.begin(), ::toupper);

            bool is_ASC = info.model_name.substr(0,4) == "ASC_";
            if (is_ASC) info.model_name.erase(0, 4);
            StateFreqType freq = FREQ_UNKNOWN;
            if (info.model_name.find_first_of("*+{") == string::npos ) {
                if (*info.model_name.rbegin() == 'F' && info.model_name != "DAYHOFF") {
                    freq = FREQ_EMPIRICAL;
                    info.model_name.erase(info.model_name.length()-1);
                } else if (*info.model_name.rbegin() == 'X' && info.model_name != "LG4X") {
                    freq = FREQ_ESTIMATE;
                    info.model_name.erase(info.model_name.length()-1);
                }
            }
            
            if (info.model_name.empty())
                outError("Please give model names in partition file!");
            if (info.model_name == "BIN") {
                info.sequence_type = "BIN";
                info.model_name = "GTR2";
            } else if (info.model_name == "DNA") {
                info.sequence_type = "DNA";
                info.model_name = "GTR";
            } else if (info.model_name == "MULTI") {
                info.sequence_type = "MORPH";
                info.model_name = "MK";
            } else if (info.model_name.substr(0,5) == "CODON") {
                info.sequence_type = info.model_name;
                info.model_name = "GY";
            } else {
                info.sequence_type = "AA";
                if (*info.model_name.begin() == '[') {
                    if (*info.model_name.rbegin() != ']')
                        outError("User-defined protein model should be [myProtenSubstitutionModelFileName]");
                    info.model_name = info.model_name.substr(1, info.model_name.length()-2);
                }
            }

            if (freq == FREQ_EMPIRICAL) 
                info.model_name += "+F";
            else if (freq == FREQ_ESTIMATE)
                info.model_name += "+FO";
            if (is_ASC)
                info.model_name += "+ASC";
            info.model_name += rate_type;

			getline(in, info.name, '=');
            trimString(info.name);
            if (info.name.empty())
                outError("Please give partition names in partition file!");

			getline(in, info.position_spec);
            trimString(info.position_spec);
            if (info.position_spec.empty())
                outError("Please specify alignment positions for partition" + info.name);
            std::replace(info.position_spec.begin(), info.position_spec.end(), ',', ' ');
            
			cout << "Reading partition " << info.name << " (model=" << info.model_name << ", seq=" << info.sequence_type << ", pos=" << ((info.position_spec.length() >= 20) ? info.position_spec.substr(0,20)+"..." : info.position_spec) << ") ..." << endl;

			//info.mem_ptnlh = NULL;
			info.nniMoves[0].ptnlh = NULL;
			info.nniMoves[1].ptnlh = NULL;
			info.cur_ptnlh = NULL;
			part_info.push_back(info);
            Alignment *part_aln = new Alignment();
            part_aln->extractSites(input_aln, info.position_spec.c_str());
            
			Alignment *new_aln;
			if (params.remove_empty_seq)
				new_aln = part_aln->removeGappySeq();
			else
				new_aln = part_aln;
		    // also rebuild states set of each sequence for likelihood computation
		    new_aln->buildSeqStates();

			if (part_aln != new_aln) delete part_aln;
			PhyloTree *tree = new PhyloTree(new_aln);
            push_back(tree);
            cout << new_aln->getNSeq() << " sequences and " << new_aln->getNSite() << " sites extracted" << endl;
//			params = origin_params;
		}

		in.clear();
		// set the failbit again
		in.exceptions(ios::failbit | ios::badbit);
		in.close();
	} catch(ios::failure) {
		outError(ERR_READ_INPUT);
	} catch (string str) {
		outError(str);
	}


}

void PhyloSuperTree::readPartitionNexus(Params &params) {
	Params origin_params = params;
	MSetsBlock *sets_block = new MSetsBlock();
    MyReader nexus(params.partition_file);
    nexus.Add(sets_block);
    MyToken token(nexus.inf);
    nexus.Execute(token);

    Alignment *input_aln = NULL;
    if (params.aln_file) {
    	input_aln = new Alignment(params.aln_file, params.sequence_type, params.intype);
    }

    bool empty_partition = true;
    vector<CharSet*>::iterator it;
    for (it = sets_block->charsets.begin(); it != sets_block->charsets.end(); it++)
    	if ((*it)->model_name != "") {
    		empty_partition = false;
    		break;
    	}
    if (empty_partition) {
    	cout << "NOTE: No CharPartition defined, use all CharSets" << endl;
    }

    for (it = sets_block->charsets.begin(); it != sets_block->charsets.end(); it++)
    	if (empty_partition || (*it)->char_partition != "") {
			PartitionInfo info;
			info.name = (*it)->name;
			info.model_name = (*it)->model_name;
			if (info.model_name == "")
				info.model_name = params.model_name;
			info.aln_file = (*it)->aln_file;
			//if (info.aln_file == "" && params.aln_file) info.aln_file = params.aln_file;
			if (info.aln_file == "" && !params.aln_file)
				outError("No input data for partition ", info.name);
			info.sequence_type = (*it)->sequence_type;
			if (info.sequence_type=="" && params.sequence_type) info.sequence_type = params.sequence_type;
            
            if (info.sequence_type == "" && !info.model_name.empty()) {
                // try to get sequence type from model
                info.sequence_type = getSeqType(info.model_name.substr(0, info.model_name.find_first_of("+*")));
            }
			info.position_spec = (*it)->position_spec;
			trimString(info.sequence_type);
			cout << endl << "Reading partition " << info.name << " (model=" << info.model_name << ", aln=" <<
				info.aln_file << ", seq=" << info.sequence_type << ", pos=" << ((info.position_spec.length() >= 20) ? info.position_spec.substr(0,20)+"..." : info.position_spec) << ") ..." << endl;
            if (info.sequence_type != "" && Alignment::getSeqType(info.sequence_type.c_str()) == SEQ_UNKNOWN)
                outError("Unknown sequence type " + info.sequence_type);
			//info.mem_ptnlh = NULL;
			info.nniMoves[0].ptnlh = NULL;
			info.nniMoves[1].ptnlh = NULL;
			info.cur_ptnlh = NULL;
			part_info.push_back(info);
			Alignment *part_aln;
			if (info.aln_file != "") {
				part_aln = new Alignment((char*)info.aln_file.c_str(), (char*)info.sequence_type.c_str(), params.intype);
			} else {
				part_aln = input_aln;
			}
			if (!info.position_spec.empty() && info.position_spec != "*") {
				Alignment *new_aln = new Alignment();
				new_aln->extractSites(part_aln, info.position_spec.c_str());
				if (part_aln != input_aln) delete part_aln;
				part_aln = new_aln;
			}
            if (part_aln->seq_type == SEQ_DNA && (info.sequence_type.substr(0, 5) == "CODON" || info.sequence_type.substr(0, 5) == "NT2AA")) {
				Alignment *new_aln = new Alignment();
                new_aln->convertToCodonOrAA(part_aln, &info.sequence_type[5], info.sequence_type.substr(0, 5) == "NT2AA");
                if (part_aln != input_aln) delete part_aln;
                part_aln = new_aln;
            }
			Alignment *new_aln;
			if (params.remove_empty_seq)
				new_aln = part_aln->removeGappySeq();
			else
				new_aln = part_aln;
		    // also rebuild states set of each sequence for likelihood computation
		    new_aln->buildSeqStates();

			if (part_aln != new_aln && part_aln != input_aln) delete part_aln;
			PhyloTree *tree = new PhyloTree(new_aln);
			push_back(tree);
			params = origin_params;
			cout << new_aln->getNSeq() << " sequences and " << new_aln->getNSite() << " sites extracted" << endl;
    	}

    if (input_aln)
    	delete input_aln;
    delete sets_block;
}

void PhyloSuperTree::printPartition(const char *filename) {
   try {
		ofstream out;
		out.exceptions(ios::failbit | ios::badbit);
		out.open(filename);
		out << "#nexus" << endl << "[ partition information for alignment written in .conaln file ]" << endl
			<< "begin sets;" << endl;
		int part; int start_site;
		for (part = 0, start_site = 1; part < part_info.size(); part++) {
			string name = part_info[part].name;
			replace(name.begin(), name.end(), '+', '_');
			int end_site = start_site + at(part)->getAlnNSite();
			out << "  charset " << name << " = " << start_site << "-" << end_site-1 << ";" << endl;
			start_site = end_site;
		}
		out << "  charpartition mymodels =" << endl;
		for (part = 0; part < part_info.size(); part++) {
			string name = part_info[part].name;
			replace(name.begin(), name.end(), '+', '_');
			if (part > 0) out << "," << endl;
			out << "    " << at(part)->getModelNameParams() << ":" << name;
		}
		out << ";" << endl;
		out << "end;" << endl;
		out.close();
		cout << "Partition information was printed to " << filename << endl;
	} catch (ios::failure &) {
		outError(ERR_WRITE_OUTPUT, filename);
	}

}

void PhyloSuperTree::printBestPartition(const char *filename) {
   try {
		ofstream out;
		out.exceptions(ios::failbit | ios::badbit);
		out.open(filename);
		out << "#nexus" << endl
			<< "begin sets;" << endl;
		int part;
		for (part = 0; part < part_info.size(); part++) {
			string name = part_info[part].name;
			replace(name.begin(), name.end(), '+', '_');
			out << "  charset " << name << " = ";
			if (!part_info[part].aln_file.empty()) out << part_info[part].aln_file << ": ";
			if (at(part)->aln->seq_type == SEQ_CODON)
				out << "CODON, ";
			string pos = part_info[part].position_spec;
			replace(pos.begin(), pos.end(), ',' , ' ');
			out << pos << ";" << endl;
		}
		out << "  charpartition mymodels =" << endl;
		for (part = 0; part < part_info.size(); part++) {
			string name = part_info[part].name;
			replace(name.begin(), name.end(), '+', '_');
			if (part > 0) out << "," << endl;
			out << "    " << part_info[part].model_name << ": " << name;
		}
		out << ";" << endl;
		out << "end;" << endl;
		out.close();
		cout << "Partition information was printed to " << filename << endl;
	} catch (ios::failure &) {
		outError(ERR_WRITE_OUTPUT, filename);
	}

}


void PhyloSuperTree::printPartitionRaxml(const char *filename) {
	int part;
	for (part = 0; part < part_info.size(); part++) {
		if (part_info[part].aln_file != "") {
			cout << "INFO: Printing partition in RAxML format is not possible" << endl;
            return;
        }
	}
   try {
		ofstream out;
		out.exceptions(ios::failbit | ios::badbit);
		out.open(filename);
		int start_site;
		for (part = 0, start_site = 1; part < part_info.size(); part++) {
			string name = part_info[part].name;
			replace(name.begin(), name.end(), '+', '_');
			int end_site = start_site + at(part)->getAlnNSite();
			switch (at(part)->aln->seq_type) {
			case SEQ_DNA: out << "DNA, "; break;
			case SEQ_BINARY: out << "BIN, "; break;
			case SEQ_MORPH: out << "MULTI, "; break;
			default: out << at(part)->getModel()->name << ","; break;
			}
			out << name << " = " << start_site << "-" << end_site-1 << endl;
			start_site = end_site;
		}
		out.close();
		cout << "Partition information in Raxml format was printed to " << filename << endl;
	} catch (ios::failure &) {
		outError(ERR_WRITE_OUTPUT, filename);
	}

}

void PhyloSuperTree::printBestPartitionRaxml(const char *filename) {
	int part;
	for (part = 0; part < part_info.size(); part++) {
		if (part_info[part].aln_file != "") {
			cout << "INFO: Printing partition in RAxML format is not possible" << endl;
            return;
        }
	}
	try {
		ofstream out;
		out.exceptions(ios::failbit | ios::badbit);
		out.open(filename);
		for (part = 0; part < part_info.size(); part++) {
			string name = part_info[part].name;
			replace(name.begin(), name.end(), '+', '_');
            if (part_info[part].model_name.find("+ASC") != string::npos)
                out << "ASC_";
			switch (at(part)->aln->seq_type) {
			case SEQ_DNA: out << "DNA"; break;
			case SEQ_BINARY: out << "BIN"; break;
			case SEQ_MORPH: out << "MULTI"; break;
            case SEQ_PROTEIN:
                out << part_info[part].model_name.substr(0, part_info[part].model_name.find_first_of("*{+"));
                break;
            case SEQ_CODON:
                out << "CODON_" << part_info[part].model_name.substr(0, part_info[part].model_name.find_first_of("*{+"));
                break;
			default: out << part_info[part].model_name; break;
			}
            if (part_info[part].model_name.find("+FO") != string::npos)
                out << "X";
            else if (part_info[part].model_name.find("+F") != string::npos)
                out << "F";
                
			out << ", " << name << " = " << part_info[part].position_spec << endl;
		}
		out.close();
		cout << "Partition information in Raxml format was printed to " << filename << endl;
	} catch (ios::failure &) {
		outError(ERR_WRITE_OUTPUT, filename);
	}

}


PhyloSuperTree::PhyloSuperTree(Params &params) :  IQTree() {
	totalNNIs = evalNNIs = 0;

	cout << "Reading partition model file " << params.partition_file << " ..." << endl;
	if (detectInputFile(params.partition_file) == IN_NEXUS) {
		readPartitionNexus(params);
        if (part_info.empty()) {
            outError("No partition found in SETS block. An example syntax looks like: \n#nexus\nbegin sets;\n  charset part1=1-100;\n  charset part2=101-300;\nend;");
        }
	} else
		readPartitionRaxml(params);
	if (part_info.empty())
		outError("No partition found");

	// Initialize the counter for evaluated NNIs on subtrees
	int part = 0;
    iterator it;
	for (it = begin(); it != end(); it++, part++) {
		part_info[part].evalNNIs = 0.0;
	}

	aln = new SuperAlignment(this);
	if (params.print_conaln) {
		string str = params.out_prefix;
		str = params.out_prefix;
		str += ".conaln";
		((SuperAlignment*)aln)->printCombinedAlignment(str.c_str());
	}

    // this is important: rescale branch length of codon partitions to be compatible with other partitions.
    // since for codon models, branch lengths = # nucleotide subst per codon site!
    rescale_codon_brlen = false;
    bool has_codon = false;
	for (it = begin(); it != end(); it++, part++) 
        if ((*it)->aln->seq_type != SEQ_CODON) {
            rescale_codon_brlen = true;
        } else 
            has_codon = true;
            
    rescale_codon_brlen &= has_codon;
    if (rescale_codon_brlen)
        cout << "NOTE: Mixed codon and other data, branch lengths of codon partitions are rescaled by 3!" << endl;
    
	cout << "Degree of missing data: " << ((SuperAlignment*)aln)->computeMissingData() << endl;
    
#ifdef _OPENMP
    if (params.num_threads > size()) {
        cout << "Info: multi-threading strategy over alignment sites" << endl;
    } else {
        cout << "Info: multi-threading strategy over partitions" << endl;
    }
#endif
	cout << endl;

}

void PhyloSuperTree::setParams(Params* params) {
	IQTree::setParams(params);
	for (iterator it = begin(); it != end(); it++) {
		(*it)->setParams(params);
	}
}

void PhyloSuperTree::initSettings(Params &params) {
	IQTree::initSettings(params);
    num_threads = (size() >= params.num_threads) ? params.num_threads : 1;
	for (iterator it = begin(); it != end(); it++) {
		(*it)->params = &params;
		(*it)->setLikelihoodKernel(params.SSE, (size() >= params.num_threads) ? 1 : params.num_threads);
		(*it)->optimize_by_newton = params.optimize_by_newton;
	}

}

void PhyloSuperTree::setLikelihoodKernel(LikelihoodKernel lk, int num_threads) {
    PhyloTree::setLikelihoodKernel(lk, (size() >= num_threads) ? num_threads : 1);
    for (iterator it = begin(); it != end(); it++)
        (*it)->setLikelihoodKernel(lk, (size() >= num_threads) ? 1 : num_threads);
}

void PhyloSuperTree::changeLikelihoodKernel(LikelihoodKernel lk) {
	PhyloTree::changeLikelihoodKernel(lk);
}

string PhyloSuperTree::getTreeString() {
	stringstream tree_stream;
	printTree(tree_stream, WT_TAXON_ID + WT_BR_LEN + WT_SORT_TAXA);
	for (iterator it = begin(); it != end(); it++)
		(*it)->printTree(tree_stream, WT_TAXON_ID + WT_BR_LEN + WT_SORT_TAXA);
	return tree_stream.str();
}

void PhyloSuperTree::readTreeString(const string &tree_string) {
	stringstream str;
	str << tree_string;
	str.seekg(0, ios::beg);
	freeNode();
	readTree(str, rooted);
    assignLeafNames();
//	setAlignment(aln);
	setRootNode(params->root);
	for (iterator it = begin(); it != end(); it++) {
		(*it)->freeNode();
		(*it)->readTree(str, rooted);
        (*it)->assignLeafNames();
//		(*it)->setAlignment((*it)->aln);
	}
	linkTrees();
//	if (isSuperTree()) {
//		((PhyloSuperTree*) this)->mapTrees();
//	}
	if (params->pll) {
		assert(0);
		pllReadNewick(getTreeString());
	}
	resetCurScore();

}


/**
 * save branch lengths into a vector
 */
void PhyloSuperTree::saveBranchLengths(DoubleVector &lenvec, int startid, PhyloNode *node, PhyloNode *dad) {
	int totalBranchNum = branchNum;
	iterator it;
	for (it = begin(); it != end(); it++) {
		totalBranchNum += (*it)->branchNum;
	}
	lenvec.resize(startid + totalBranchNum);

	PhyloTree::saveBranchLengths(lenvec, startid);
	startid += branchNum;
	for (iterator it = begin(); it != end(); it++) {
		(*it)->saveBranchLengths(lenvec, startid);
		startid += (*it)->branchNum;
	}
}
/**
 * restore branch lengths from a vector previously called with saveBranchLengths
 */
void PhyloSuperTree::restoreBranchLengths(DoubleVector &lenvec, int startid, PhyloNode *node, PhyloNode *dad) {
	PhyloTree::restoreBranchLengths(lenvec, startid);
	startid += branchNum;
	for (iterator it = begin(); it != end(); it++) {
		(*it)->restoreBranchLengths(lenvec, startid);
		startid += (*it)->branchNum;
	}
}

Node* PhyloSuperTree::newNode(int node_id, const char* node_name) {
    return (Node*) (new SuperNode(node_id, node_name));
}

Node* PhyloSuperTree::newNode(int node_id, int node_name) {
    return (Node*) (new SuperNode(node_id, node_name));
}

int PhyloSuperTree::getAlnNPattern() {
	int num = 0;
	for (iterator it = begin(); it != end(); it++)
		num += (*it)->getAlnNPattern();
	return num;
}

int PhyloSuperTree::getAlnNSite() {
	int num = 0;
	for (iterator it = begin(); it != end(); it++)
		num += (*it)->getAlnNSite();
	return num;
}

double PhyloSuperTree::computeDist(int seq1, int seq2, double initial_dist, double &var) {
    // if no model or site rate is specified, return JC distance
    if (initial_dist == 0.0) {
    	if (params->compute_obs_dist)
            initial_dist = aln->computeObsDist(seq1, seq2);
    	else
    		initial_dist = aln->computeDist(seq1, seq2);
    }
    if (initial_dist == MAX_GENETIC_DIST) return initial_dist; // MANUEL: here no d2l is return
    if (!model_factory || !site_rate) return initial_dist; // MANUEL: here no d2l is return

    // now optimize the distance based on the model and site rate
    SuperAlignmentPairwise aln_pair(this, seq1, seq2);
    return aln_pair.optimizeDist(initial_dist, var);
}

void PhyloSuperTree::linkBranch(int part, SuperNeighbor *nei, SuperNeighbor *dad_nei) {
	SuperNode *node = (SuperNode*)dad_nei->node;
	SuperNode *dad = (SuperNode*)nei->node;
	nei->link_neighbors[part] = NULL;
	dad_nei->link_neighbors[part] = NULL;
	vector<PhyloNeighbor*> part_vec;
	vector<PhyloNeighbor*> child_part_vec;

	FOR_NEIGHBOR_DECLARE(node, dad, it) {
		if (((SuperNeighbor*)*it)->link_neighbors[part]) {
			part_vec.push_back(((SuperNeighbor*)*it)->link_neighbors[part]);
			child_part_vec.push_back(((SuperNeighbor*)(*it)->node->findNeighbor(node))->link_neighbors[part]);
			assert(child_part_vec.back()->node == child_part_vec.front()->node || child_part_vec.back()->id == child_part_vec.front()->id);
		}
	}

	if (part_vec.empty())
		return;
	if (part_vec.size() == 1) {
		nei->link_neighbors[part] = child_part_vec[0];
		dad_nei->link_neighbors[part] = part_vec[0];
		return;
	}
	if (part_vec[0] == child_part_vec[1]) {
		// ping-pong, out of sub-tree
		assert(part_vec[1] == child_part_vec[0]);
		return;
	}
	PhyloNode *node_part = (PhyloNode*) child_part_vec[0]->node;
	PhyloNode *dad_part = NULL;
	FOR_NEIGHBOR(node_part, NULL, it) {
		bool appear = false;
		for (vector<PhyloNeighbor*>::iterator it2 = part_vec.begin(); it2 != part_vec.end(); it2++){
			if ((*it2) == (*it)) {
				appear = true; break;
			}
		}
		if (!appear) {
			assert(!dad_part);
			dad_part = (PhyloNode*)(*it)->node;
		}
	}
	nei->link_neighbors[part] = (PhyloNeighbor*)node_part->findNeighbor(dad_part);
	dad_nei->link_neighbors[part] = (PhyloNeighbor*)dad_part->findNeighbor(node_part);
}

void PhyloSuperTree::linkTree(int part, NodeVector &part_taxa, SuperNode *node, SuperNode *dad) {
	if (!node) {
		if (!root->isLeaf())
			node = (SuperNode*) root;
		else
			node = (SuperNode*)root->neighbors[0]->node;
		assert(node);
		if (node->isLeaf()) // two-taxa tree
			dad = (SuperNode*)node->neighbors[0]->node;
	}
	SuperNeighbor *nei = NULL;
	SuperNeighbor *dad_nei = NULL;
	if (dad) {
		nei = (SuperNeighbor*)node->findNeighbor(dad);
		dad_nei = (SuperNeighbor*)dad->findNeighbor(node);
		if (nei->link_neighbors.empty()) nei->link_neighbors.resize(size());
		if (dad_nei->link_neighbors.empty()) dad_nei->link_neighbors.resize(size());
		nei->link_neighbors[part] = NULL;
		dad_nei->link_neighbors[part] = NULL;
	}
	if (node->isLeaf()) {
		assert(dad);
		PhyloNode *node_part = (PhyloNode*)part_taxa[node->id];
		if (node_part) {
			PhyloNode *dad_part = (PhyloNode*)node_part->neighbors[0]->node;
			assert(node_part->isLeaf());
			nei->link_neighbors[part] = (PhyloNeighbor*) node_part->neighbors[0];
			dad_nei->link_neighbors[part] = (PhyloNeighbor*)dad_part->findNeighbor(node_part);
		}
		return;
	}

	FOR_NEIGHBOR_DECLARE(node, dad, it) {
		linkTree(part, part_taxa, (SuperNode*) (*it)->node, (SuperNode*) node);
	}
	if (!dad) return;
	linkBranch(part, nei, dad_nei);
}

void PhyloSuperTree::printMapInfo() {
	NodeVector nodes1, nodes2;
	getBranches(nodes1, nodes2);
	int part = 0;
	for (iterator it = begin(); it != end(); it++, part++) {
		cout << "Subtree for partition " << part << endl;
		(*it)->drawTree(cout, WT_BR_SCALE | WT_INT_NODE | WT_TAXON_ID | WT_NEWLINE);
		for (int i = 0; i < nodes1.size(); i++) {
			PhyloNeighbor *nei1 = ((SuperNeighbor*)nodes1[i]->findNeighbor(nodes2[i]))->link_neighbors[part];
			PhyloNeighbor *nei2 = ((SuperNeighbor*)nodes2[i]->findNeighbor(nodes1[i]))->link_neighbors[part];
			cout << nodes1[i]->findNeighbor(nodes2[i])->id << ":";
			if (nodes1[i]->isLeaf()) cout << nodes1[i]->name; else cout << nodes1[i]->id;
			cout << ",";
			if (nodes2[i]->isLeaf()) cout << nodes2[i]->name; else cout << nodes2[i]->id;
			cout << " -> ";
			if (nei2) {
				cout << nei2->id << ":";
				if (nei2->node->isLeaf())
					cout << nei2->node->name;
				else cout << nei2->node->id;
			}
			else cout << -1;
			cout << ",";
			if (nei1)
				if (nei1->node->isLeaf())
					cout << nei1->node->name;
				else cout << nei1->node->id;
			else cout << -1;
			cout << endl;
		}
	}
}


void PhyloSuperTree::mapTrees() {
	assert(root);
	int part = 0, i;
	if (verbose_mode >= VB_DEBUG)
		drawTree(cout,  WT_BR_SCALE | WT_INT_NODE | WT_TAXON_ID | WT_NEWLINE | WT_BR_ID);
	for (iterator it = begin(); it != end(); it++, part++) {
		string taxa_set = aln->getPattern(part);
		(*it)->copyTree(this, taxa_set);
        if ((*it)->getModel()) {
			(*it)->initializeAllPartialLh();
        }
        (*it)->resetCurScore();
		NodeVector my_taxa, part_taxa;
		(*it)->getOrderedTaxa(my_taxa);
		part_taxa.resize(leafNum, NULL);
		for (i = 0; i < leafNum; i++) {
			int id = ((SuperAlignment*)aln)->taxa_index[i][part];
			if (id >=0) part_taxa[i] = my_taxa[id];
		}
		if (verbose_mode >= VB_DEBUG) {
			cout << "Subtree for partition " << part << endl;
			(*it)->drawTree(cout,  WT_BR_SCALE | WT_INT_NODE | WT_TAXON_ID | WT_NEWLINE | WT_BR_ID);
		}
		linkTree(part, part_taxa);
	}

	if (verbose_mode >= VB_DEBUG) printMapInfo();
}

void PhyloSuperTree::linkTrees() {
	int part = 0;
	iterator it;
	for (it = begin(), part = 0; it != end(); it++, part++) {
		(*it)->initializeTree();
		(*it)->setAlignment((*it)->aln);
        if ((*it)->getModel()) {
			(*it)->initializeAllPartialLh();
        }
        (*it)->resetCurScore();
		NodeVector my_taxa, part_taxa;
		(*it)->getOrderedTaxa(my_taxa);
		part_taxa.resize(leafNum, NULL);
		int i;
		for (i = 0; i < leafNum; i++) {
			int id = ((SuperAlignment*)aln)->taxa_index[i][part];
			if (id >=0) part_taxa[i] = my_taxa[id];
		}
		linkTree(part, part_taxa);
	}
}

void PhyloSuperTree::initializeAllPartialLh() {
	for (iterator it = begin(); it != end(); it++) {
		(*it)->initializeAllPartialLh();
	}
}


void PhyloSuperTree::deleteAllPartialLh() {
	for (iterator it = begin(); it != end(); it++) {
		(*it)->deleteAllPartialLh();
	}
}

void PhyloSuperTree::clearAllPartialLH(bool make_null) {
    for (iterator it = begin(); it != end(); it++) {
        (*it)->clearAllPartialLH(make_null);
    }
}

int PhyloSuperTree::computeParsimonyBranchObsolete(PhyloNeighbor *dad_branch, PhyloNode *dad, int *branch_subst) {
    int score = 0, part = 0;
    SuperNeighbor *dad_nei = (SuperNeighbor*)dad_branch;
    SuperNeighbor *node_nei = (SuperNeighbor*)(dad_branch->node->findNeighbor(dad));
        
    if (branch_subst)
        branch_subst = 0;
    for (iterator it = begin(); it != end(); it++, part++) {
        int this_subst = 0;
        if (dad_nei->link_neighbors[part]) {
            if (branch_subst)
                score += (*it)->computeParsimonyBranch(dad_nei->link_neighbors[part], (PhyloNode*)node_nei->link_neighbors[part]->node, &this_subst);
            else
                score += (*it)->computeParsimonyBranch(dad_nei->link_neighbors[part], (PhyloNode*)node_nei->link_neighbors[part]->node);
        } else
            score += (*it)->computeParsimony();
        if (branch_subst)
            branch_subst += this_subst;
    }
    return score;
}

void PhyloSuperTree::computePartitionOrder() {
    if (!part_order.empty())
        return;
    int i, ntrees = size();
    part_order.resize(ntrees);
    part_order_by_nptn.resize(ntrees);
#ifdef _OPENMP
    int *id = new int[ntrees];
    double *cost = new double[ntrees];
    
    for (i = 0; i < ntrees; i++) {
        Alignment *part_aln = at(i)->aln;
        cost[i] = -((double)part_aln->getNSeq())*part_aln->getNPattern()*part_aln->num_states;
        id[i] = i;
    }
    quicksort(cost, 0, ntrees-1, id);
    for (i = 0; i < ntrees; i++) 
        part_order[i] = id[i];
        
    // compute part_order by number of patterns
    for (i = 0; i < ntrees; i++) {
        Alignment *part_aln = at(i)->aln;
        cost[i] = -((double)part_aln->getNPattern())*part_aln->num_states;
        id[i] = i;
    }
    quicksort(cost, 0, ntrees-1, id);
    for (i = 0; i < ntrees; i++) 
        part_order_by_nptn[i] = id[i];
        
    delete [] cost;
    delete [] id;
#else
    for (i = 0; i < ntrees; i++) {
        part_order[i] = i;
        part_order_by_nptn[i] = i;
    }
#endif // OPENMP
}

double PhyloSuperTree::computeLikelihood(double *pattern_lh) {
	double tree_lh = 0.0;
	int ntrees = size();
	if (pattern_lh) {
		//#ifdef _OPENMP
		//#pragma omp parallel for reduction(+: tree_lh)
		//#endif
		for (int i = 0; i < ntrees; i++) {
			part_info[i].cur_score = at(i)->computeLikelihood(pattern_lh);
			tree_lh += part_info[i].cur_score;
			pattern_lh += at(i)->getAlnNPattern();
		}
	} else {
        if (part_order.empty()) computePartitionOrder();
		#ifdef _OPENMP
		#pragma omp parallel for reduction(+: tree_lh) schedule(dynamic) if(num_threads > 1)
		#endif
		for (int j = 0; j < ntrees; j++) {
            int i = part_order[j];
			part_info[i].cur_score = at(i)->computeLikelihood();
			tree_lh += part_info[i].cur_score;
		}
	}
	return tree_lh;
}

int PhyloSuperTree::getNumLhCat(SiteLoglType wsl) {
    int ncat = 0, it_cat;
    for (iterator it = begin(); it != end(); it++)
        if ((it_cat = (*it)->getNumLhCat(wsl)) > ncat)
            ncat = it_cat;
    return ncat;
}


void PhyloSuperTree::computePatternLikelihood(double *pattern_lh, double *cur_logl, double *ptn_lh_cat, SiteLoglType wsl) {
	size_t offset = 0, offset_lh_cat = 0;
	iterator it;
	for (it = begin(); it != end(); it++) {
		if (ptn_lh_cat)
			(*it)->computePatternLikelihood(pattern_lh + offset, NULL, ptn_lh_cat + offset_lh_cat, wsl);
		else
			(*it)->computePatternLikelihood(pattern_lh + offset);
		offset += (*it)->aln->getNPattern();
        offset_lh_cat += (*it)->aln->getNPattern() * (*it)->getNumLhCat(wsl);
	}
	if (cur_logl) { // sanity check
		double sum_logl = 0;
		offset = 0;
		for (it = begin(); it != end(); it++) {
			int nptn = (*it)->aln->getNPattern();
			for (int j = 0; j < nptn; j++)
				sum_logl += pattern_lh[offset + j] * (*it)->aln->at(j).frequency;
			offset += (*it)->aln->getNPattern();
		}
		if (fabs(sum_logl - *cur_logl) > 0.001) {
            cout << *cur_logl << " " << sum_logl << endl;
//            outError("Wrong PhyloSuperTree::", __func__);
		}
        assert(fabs(sum_logl - *cur_logl) < 0.001);
	}
}

void PhyloSuperTree::computePatternProbabilityCategory(double *ptn_prob_cat, SiteLoglType wsl) {
	size_t offset = 0;
	for (iterator it = begin(); it != end(); it++) {
        (*it)->computePatternProbabilityCategory(ptn_prob_cat + offset, wsl);
        offset += (*it)->aln->getNPattern() * (*it)->getNumLhCat(wsl);
	}
}

double PhyloSuperTree::optimizeAllBranches(int my_iterations, double tolerance, int maxNRStep) {
	double tree_lh = 0.0;
	int ntrees = size();
    if (part_order.empty()) computePartitionOrder();
	#ifdef _OPENMP
	#pragma omp parallel for reduction(+: tree_lh) schedule(dynamic) if(num_threads > 1)
	#endif
	for (int j = 0; j < ntrees; j++) {
        int i = part_order[j];
		part_info[i].cur_score = at(i)->optimizeAllBranches(my_iterations, tolerance/min(ntrees,10), maxNRStep);
		tree_lh += part_info[i].cur_score;
		if (verbose_mode >= VB_MAX)
			at(i)->printTree(cout, WT_BR_LEN + WT_NEWLINE);
	}

	if (my_iterations >= 100) computeBranchLengths();
	return tree_lh;
}

PhyloSuperTree::~PhyloSuperTree()
{
	for (vector<PartitionInfo>::reverse_iterator pit = part_info.rbegin(); pit != part_info.rend(); pit++) {
		if (pit->nniMoves[1].ptnlh)
			delete [] pit->nniMoves[1].ptnlh;
		pit->nniMoves[1].ptnlh = NULL;
		if (pit->nniMoves[0].ptnlh)
			delete [] pit->nniMoves[0].ptnlh;
		pit->nniMoves[0].ptnlh = NULL;
		if (pit->cur_ptnlh)
			delete [] pit->cur_ptnlh;
		pit->cur_ptnlh = NULL;
	}
	part_info.clear();

	for (reverse_iterator it = rbegin(); it != rend(); it++)
		delete (*it);
	clear();
}


void PhyloSuperTree::initPartitionInfo() {
	int part = 0;
	for (iterator it = begin(); it != end(); it++, part++) {
		part_info[part].cur_score = 0.0;

		part_info[part].cur_brlen.resize((*it)->branchNum, 0.0);
		if (params->nni5) {
			part_info[part].nni1_brlen.resize((*it)->branchNum * 5, 0.0);
			part_info[part].nni2_brlen.resize((*it)->branchNum * 5, 0.0);
		} else {
			part_info[part].nni1_brlen.resize((*it)->branchNum, 0.0);
			part_info[part].nni2_brlen.resize((*it)->branchNum, 0.0);
		}

		(*it)->getBranchLengths(part_info[part].cur_brlen);

		if (save_all_trees == 2 || params->write_intermediate_trees >= 2) {
			// initialize ptnlh for ultrafast bootstrap
			int nptn = (*it)->getAlnNPattern();
			if (!part_info[part].cur_ptnlh)
				part_info[part].cur_ptnlh = new double[nptn];
			if (!part_info[part].nniMoves[0].ptnlh)
				part_info[part].nniMoves[0].ptnlh = new double [nptn];
			if (!part_info[part].nniMoves[1].ptnlh)
				part_info[part].nniMoves[1].ptnlh = new double [nptn];
		}
	}
}

int PhyloSuperTree::getMaxPartNameLength() {
	int namelen = 0;
	for (vector<PartitionInfo>::iterator it = part_info.begin(); it != part_info.end(); it++)
		namelen = max((int)it->name.length(), namelen);
	return namelen;
}

NNIMove PhyloSuperTree::getBestNNIForBran(PhyloNode *node1, PhyloNode *node2, NNIMove *nniMoves) {
    NNIMove myMove;
    //myMove.newloglh = 0;
	SuperNeighbor *nei1 = ((SuperNeighbor*)node1->findNeighbor(node2));
	SuperNeighbor *nei2 = ((SuperNeighbor*)node2->findNeighbor(node1));
	assert(nei1 && nei2);
	SuperNeighbor *node1_nei = NULL;
	SuperNeighbor *node2_nei = NULL;
	SuperNeighbor *node2_nei_other = NULL;
	FOR_NEIGHBOR_DECLARE(node1, node2, node1_it) {
		node1_nei = (SuperNeighbor*)(*node1_it);
		break;
	}
	FOR_NEIGHBOR_DECLARE(node2, node1, node2_it) {
		node2_nei = (SuperNeighbor*)(*node2_it);
		break;
	}

	FOR_NEIGHBOR_IT(node2, node1, node2_it_other)
	if ((*node2_it_other) != node2_nei) {
		node2_nei_other = (SuperNeighbor*)(*node2_it_other);
		break;
	}

    // check for compatibility with constraint tree
    bool nni_ok[2] = {true, true};
    int nniid = 0;
	FOR_NEIGHBOR(node2, node1, node2_it) {
        NNIMove nni;
        nni.node1 = node1;
        nni.node2 = node2;
        nni.node1Nei_it = node1->findNeighborIt(node1_nei->node);
        nni.node2Nei_it = node2_it;
        nni_ok[nniid++] = constraintTree.isCompatible(nni);
    }
    assert(nniid == 2);
    myMove.node1 = myMove.node2 = NULL;
    myMove.newloglh = -DBL_MAX;
    // return if both NNIs do not satisfy constraint
    if (!nni_ok[0] && !nni_ok[1]) {
        assert(!nniMoves);
        return myMove;
    }

	//double bestScore = optimizeOneBranch(node1, node2, false);

	int ntrees = size(), part;
	double nni_score1 = 0.0, nni_score2 = 0.0;
	int local_totalNNIs = 0, local_evalNNIs = 0;

    if (part_order.empty()) computePartitionOrder();
	#ifdef _OPENMP
	#pragma omp parallel for reduction(+: nni_score1, nni_score2, local_totalNNIs, local_evalNNIs) private(part) schedule(dynamic) if(num_threads>1)
	#endif
	for (int treeid = 0; treeid < ntrees; treeid++) {
        part = part_order_by_nptn[treeid];
		bool is_nni = true;
		local_totalNNIs++;
		FOR_NEIGHBOR_DECLARE(node1, NULL, nit) {
			if (! ((SuperNeighbor*)*nit)->link_neighbors[part]) { is_nni = false; break; }
		}
		FOR_NEIGHBOR(node2, NULL, nit) {
			if (! ((SuperNeighbor*)*nit)->link_neighbors[part]) { is_nni = false; break; }
		}
		if (!is_nni && params->terrace_aware) {
			if (part_info[part].cur_score == 0.0)  {
				part_info[part].cur_score = at(part)->computeLikelihood();
				if (save_all_trees == 2 || nniMoves)
					at(part)->computePatternLikelihood(part_info[part].cur_ptnlh, &part_info[part].cur_score);
			}
			nni_score1 += part_info[part].cur_score;
			nni_score2 += part_info[part].cur_score;
			continue;
		}

		local_evalNNIs++;
		part_info[part].evalNNIs++;

		PhyloNeighbor *nei1_part = nei1->link_neighbors[part];
		PhyloNeighbor *nei2_part = nei2->link_neighbors[part];

		int brid = nei1_part->id;

		//NNIMove part_moves[2];
		//part_moves[0].node1Nei_it = NULL;

		// setup subtree NNI correspondingly
		PhyloNode *node1_part = (PhyloNode*)nei2_part->node;
		PhyloNode *node2_part = (PhyloNode*)nei1_part->node;
		part_info[part].nniMoves[0].node1 = part_info[part].nniMoves[1].node1 = node1;
		part_info[part].nniMoves[0].node2 = part_info[part].nniMoves[1].node2 = node2;
		part_info[part].nniMoves[0].node1Nei_it = node1_part->findNeighborIt(node1_nei->link_neighbors[part]->node);
		part_info[part].nniMoves[0].node2Nei_it = node2_part->findNeighborIt(node2_nei->link_neighbors[part]->node);

		part_info[part].nniMoves[1].node1Nei_it = node1_part->findNeighborIt(node1_nei->link_neighbors[part]->node);
		part_info[part].nniMoves[1].node2Nei_it = node2_part->findNeighborIt(node2_nei_other->link_neighbors[part]->node);

		at(part)->getBestNNIForBran((PhyloNode*)nei2_part->node, (PhyloNode*)nei1_part->node, part_info[part].nniMoves);
		// detect the corresponding NNIs and swap if necessary (the swapping refers to the swapping of NNI order)
		if (!((*part_info[part].nniMoves[0].node1Nei_it == node1_nei->link_neighbors[part] &&
				*part_info[part].nniMoves[0].node2Nei_it == node2_nei->link_neighbors[part]) ||
			(*part_info[part].nniMoves[0].node1Nei_it != node1_nei->link_neighbors[part] &&
					*part_info[part].nniMoves[0].node2Nei_it != node2_nei->link_neighbors[part])))
		{
			outError("WRONG");
			NNIMove tmp = part_info[part].nniMoves[0];
			part_info[part].nniMoves[0] = part_info[part].nniMoves[1];
			part_info[part].nniMoves[1] = tmp;
		}
		nni_score1 += part_info[part].nniMoves[0].newloglh;
		nni_score2 += part_info[part].nniMoves[1].newloglh;
		int numlen = 1;
		if (params->nni5) numlen = 5;
		for (int i = 0; i < numlen; i++) {
			part_info[part].nni1_brlen[brid*numlen + i] = part_info[part].nniMoves[0].newLen[i];
			part_info[part].nni2_brlen[brid*numlen + i] = part_info[part].nniMoves[1].newLen[i];
		}

	}
	totalNNIs += local_totalNNIs;
	evalNNIs += local_evalNNIs;
	double nni_scores[2] = {nni_score1, nni_score2};
    
    if (!nni_ok[0]) nni_scores[0] = -DBL_MAX;
    if (!nni_ok[1]) nni_scores[1] = -DBL_MAX;

	myMove.node1Nei_it = node1->findNeighborIt(node1_nei->node);
	myMove.node1 = node1;
	myMove.node2 = node2;
	if (nni_scores[0] > nni_scores[1]) {
		myMove.swap_id = 1;
		myMove.node2Nei_it = node2->findNeighborIt(node2_nei->node);
		myMove.newloglh = nni_scores[0];
	} else  {
		myMove.swap_id = 2;
		myMove.node2Nei_it = node2->findNeighborIt(node2_nei_other->node);
		myMove.newloglh = nni_scores[1];
	}

	if (save_all_trees != 2 && !nniMoves) return myMove;

	// for bootstrap now
    //now setup pattern likelihoods per partition
	double *save_lh_factor = new double [ntrees];
	double *save_lh_factor_back = new double [ntrees];
	nniid = 0;
	FOR_NEIGHBOR(node2, node1, node2_it) if (nni_ok[nniid]) 
    {

		// do the NNI
		node2_nei = (SuperNeighbor*)(*node2_it);
        node1->updateNeighbor(node1_it, node2_nei);
        node2_nei->node->updateNeighbor(node2, node1);
        node2->updateNeighbor(node2_it, node1_nei);
        node1_nei->node->updateNeighbor(node1, node2);

        for (part = 0; part < ntrees; part++) {
			bool is_nni = true;
			FOR_NEIGHBOR_DECLARE(node1, NULL, nit) {
				if (! ((SuperNeighbor*)*nit)->link_neighbors[part]) { is_nni = false; break; }
			}
			FOR_NEIGHBOR(node2, NULL, nit) {
				if (! ((SuperNeighbor*)*nit)->link_neighbors[part]) { is_nni = false; break; }
			}
			if (!is_nni)
				memcpy(at(part)->_pattern_lh, part_info[part].cur_ptnlh, at(part)->getAlnNPattern() * sizeof(double));
			else
				memcpy(at(part)->_pattern_lh, part_info[part].nniMoves[nniid].ptnlh, at(part)->getAlnNPattern() * sizeof(double));
    		save_lh_factor[part] = at(part)->current_it->lh_scale_factor;
    		save_lh_factor_back[part] = at(part)->current_it_back->lh_scale_factor;
    		at(part)->current_it->lh_scale_factor = 0.0;
    		at(part)->current_it_back->lh_scale_factor = 0.0;
        }
        if (nniMoves) {
        	nniMoves[nniid].newloglh = nni_scores[nniid];
       		computePatternLikelihood(nniMoves[nniid].ptnlh, &nni_scores[nniid]);
        }
        if (save_all_trees == 2)
        	saveCurrentTree(nni_scores[nniid]);

        // restore information
        for (part = 0; part < ntrees; part++) {
    		at(part)->current_it->lh_scale_factor = save_lh_factor[part];
    		at(part)->current_it_back->lh_scale_factor = save_lh_factor_back[part];
        }

        // swap back to recover the tree
        node1->updateNeighbor(node1_it, node1_nei);
        node1_nei->node->updateNeighbor(node2, node1);
        node2->updateNeighbor(node2_it, node2_nei);
        node2_nei->node->updateNeighbor(node1, node2);
        nniid++;

	}

	delete [] save_lh_factor_back;
	delete [] save_lh_factor;
	return myMove;
}

void PhyloSuperTree::doNNI(NNIMove &move, bool clearLH) {
	SuperNeighbor *nei1 = (SuperNeighbor*)move.node1->findNeighbor(move.node2);
	SuperNeighbor *nei2 = (SuperNeighbor*)move.node2->findNeighbor(move.node1);
	SuperNeighbor *node1_nei = (SuperNeighbor*)*move.node1Nei_it;
	SuperNeighbor *node2_nei = (SuperNeighbor*)*move.node2Nei_it;
	int part = 0;
	iterator it;
	PhyloTree::doNNI(move, clearLH);

	for (it = begin(), part = 0; it != end(); it++, part++) {
		bool is_nni = true;
		FOR_NEIGHBOR_DECLARE(move.node1, NULL, nit) {
			if (! ((SuperNeighbor*)*nit)->link_neighbors[part]) { is_nni = false; break; }
		}
		FOR_NEIGHBOR(move.node2, NULL, nit) {
			if (! ((SuperNeighbor*)*nit)->link_neighbors[part]) { is_nni = false; break; }
		}
		if (!is_nni) {
			// relink the branch if it does not correspond to NNI for partition
			linkBranch(part, nei1, nei2);
			continue;
		}

		NNIMove part_move;
		PhyloNeighbor *nei1_part = nei1->link_neighbors[part];
		PhyloNeighbor *nei2_part = nei2->link_neighbors[part];
		part_move.node1 = (PhyloNode*)nei2_part->node;
		part_move.node2 = (PhyloNode*)nei1_part->node;
		part_move.node1Nei_it = part_move.node1->findNeighborIt(node1_nei->link_neighbors[part]->node);
		part_move.node2Nei_it = part_move.node2->findNeighborIt(node2_nei->link_neighbors[part]->node);

		(*it)->doNNI(part_move, clearLH);

	}

}

void PhyloSuperTree::changeNNIBrans(NNIMove move) {
	SuperNeighbor *nei1 = (SuperNeighbor*)move.node1->findNeighbor(move.node2);
	SuperNeighbor *nei2 = (SuperNeighbor*)move.node2->findNeighbor(move.node1);
	iterator it;
	int part;

	for (it = begin(), part = 0; it != end(); it++, part++) {
		bool is_nni = true;
		FOR_NEIGHBOR_DECLARE(move.node1, NULL, nit) {
			if (! ((SuperNeighbor*)*nit)->link_neighbors[part]) { is_nni = false; break; }
		}
		FOR_NEIGHBOR(move.node2, NULL, nit) {
			if (! ((SuperNeighbor*)*nit)->link_neighbors[part]) { is_nni = false; break; }
		}
		if (!is_nni) {
			continue;
		}

		NNIMove part_move;
		PhyloNeighbor *nei1_part = nei1->link_neighbors[part];
		PhyloNeighbor *nei2_part = nei2->link_neighbors[part];
		int brid = nei1_part->id;
		part_move.node1 = (PhyloNode*)nei2_part->node;
		part_move.node2 = (PhyloNode*)nei1_part->node;
		int numlen = 1;
		if (params->nni5) numlen = 5;
		if (move.swap_id == 1) {
			for (int i = 0; i < numlen; i++)
				part_move.newLen[i] = part_info[part].nni1_brlen[brid*numlen + i];
		} else {
			for (int i = 0; i < numlen; i++)
				part_move.newLen[i] = part_info[part].nni2_brlen[brid*numlen + i];
		}

		(*it)->changeNNIBrans(part_move);

	}

}

//void PhyloSuperTree::restoreAllBrans(PhyloNode *node, PhyloNode *dad) {
//	int part = 0;
//	for (iterator it = begin(); it != end(); it++, part++) {
//		(*it)->setBranchLengths(part_info[part].cur_brlen);
//	}
//}

void PhyloSuperTree::reinsertLeaves(PhyloNodeVector &del_leaves) {
	IQTree::reinsertLeaves(del_leaves);
	mapTrees();
}

void PhyloSuperTree::computeBranchLengths() {
	if (verbose_mode >= VB_DEBUG)
		cout << "Assigning branch lengths for full tree with weighted average..." << endl;
	int part = 0, i;
    iterator it;

	NodeVector nodes1, nodes2;
	getBranches(nodes1, nodes2);
	vector<SuperNeighbor*> neighbors1;
	vector<SuperNeighbor*> neighbors2;
	IntVector occurence;
	occurence.resize(nodes1.size(), 0);
	for (i = 0; i < nodes1.size(); i++) {
		neighbors1.push_back((SuperNeighbor*)nodes1[i]->findNeighbor(nodes2[i]) );
		neighbors2.push_back((SuperNeighbor*)nodes2[i]->findNeighbor(nodes1[i]) );
		neighbors1.back()->length = 0.0;
	}
	for (it = begin(), part = 0; it != end(); it++, part++) {
		IntVector brfreq;
		brfreq.resize((*it)->branchNum, 0);
		for (i = 0; i < nodes1.size(); i++) {
			PhyloNeighbor *nei1 = neighbors1[i]->link_neighbors[part];
			if (!nei1) continue;
			brfreq[nei1->id]++;
		}
		for (i = 0; i < nodes1.size(); i++) {
			PhyloNeighbor *nei1 = neighbors1[i]->link_neighbors[part];
			if (!nei1) continue;
            if ((*it)->aln->seq_type == SEQ_CODON && rescale_codon_brlen) {
                // rescale branch length by 3
                neighbors1[i]->length += (nei1->length) * (*it)->aln->getNSite() / brfreq[nei1->id];
                occurence[i] += (*it)->aln->getNSite()*3;
            } else {
                neighbors1[i]->length += (nei1->length) * (*it)->aln->getNSite() / brfreq[nei1->id];
                occurence[i] += (*it)->aln->getNSite();
            }
			//cout << neighbors1[i]->id << "  " << nodes1[i]->id << nodes1[i]->name <<"," << nodes2[i]->id << nodes2[i]->name <<": " << (nei1->length) / brfreq[nei1->id] << endl;
		}
		//cout << endl;
	}
	for (i = 0; i < nodes1.size(); i++) {
		if (occurence[i])
			neighbors1[i]->length /= occurence[i];
		neighbors2[i]->length = neighbors1[i]->length;
	}
}

string PhyloSuperTree::getModelName() {
	return (string)"Partition model";
}

PhyloTree *PhyloSuperTree::extractSubtree(IntVector &ids) {
	string union_taxa;
	int i;
	for (i = 0; i < ids.size(); i++) {
		int id = ids[i];
		if (id < 0 || id >= size())
			outError("Internal error ", __func__);
		string taxa_set = aln->getPattern(id);
		if (i == 0) union_taxa = taxa_set; else {
			for (int j = 0; j < union_taxa.length(); j++)
				if (taxa_set[j] == 1) union_taxa[j] = 1;
		}
	}
	PhyloTree *tree = new PhyloTree;
	tree->copyTree(this, union_taxa);
	return tree;
}

uint64_t PhyloSuperTree::getMemoryRequired(size_t ncategory, bool full_mem) {
//	uint64_t mem_size = PhyloTree::getMemoryRequired(ncategory);
	// supertree does not need any memory for likelihood vectors!
	uint64_t mem_size = 0;
	for (iterator it = begin(); it != end(); it++)
		mem_size += (*it)->getMemoryRequired(ncategory, full_mem);
	return mem_size;
}

int PhyloSuperTree::countEmptyBranches(PhyloNode *node, PhyloNode *dad) {
	int count = 0;
    if (!node)
        node = (PhyloNode*)root;

    FOR_NEIGHBOR_IT(node, dad, it) {
    	SuperNeighbor *nei = (SuperNeighbor*)(*it);
    	bool isempty = true;
    	for (PhyloNeighborVec::iterator nit = nei->link_neighbors.begin(); nit != nei->link_neighbors.end(); nit++)
    		if ((*nit)) {
    			isempty = false;
    			break;
    		}
    	if (isempty) count++;
    	count += countEmptyBranches((PhyloNode*)(*it)->node, node);
    }
    return count;
}

/** remove identical sequences from the tree */
void PhyloSuperTree::removeIdenticalSeqs(Params &params) {
	IQTree::removeIdenticalSeqs(params);
	if (removed_seqs.empty()) return;
	// now synchronize aln
	int part = 0;
	for (iterator it = begin(); it != end(); it++, part++) {
		if (verbose_mode >= VB_MED) {
			cout << "Partition " << part_info[part].name << " " << ((SuperAlignment*)aln)->partitions[part]->getNSeq() <<
					" sequences from " << (*it)->aln->getNSeq() << " extracted" << endl;
		}
		(*it)->aln = ((SuperAlignment*)aln)->partitions[part];
	}
	if (verbose_mode >= VB_MED) {
		cout << "Reduced alignment has " << aln->getNSeq() << " sequences with " << getAlnNSite() << " sites and "
				<< getAlnNPattern() << " patterns" << endl;
	}

}

/** reinsert identical sequences into the tree and reset original alignment */
void PhyloSuperTree::reinsertIdenticalSeqs(Alignment *orig_aln) {
	if (removed_seqs.empty()) return;
	IQTree::reinsertIdenticalSeqs(orig_aln);

	// now synchronize aln
	int part = 0;
    for (iterator it = begin(); it != end(); it++, part++) {
//        (*it)->setAlignment(((SuperAlignment*)aln)->partitions[part]);
		(*it)->aln = ((SuperAlignment*)aln)->partitions[part];
    }
	mapTrees();


}

int PhyloSuperTree::fixNegativeBranch(bool force, Node *node, Node *dad) {
	mapTrees();
	int fixed = 0;
	for (iterator it = begin(); it != end(); it++) {
		(*it)->initializeAllPartialPars();
		(*it)->clearAllPartialLH();
		fixed += (*it)->fixNegativeBranch(force);
		(*it)->clearAllPartialLH();
	}
	computeBranchLengths();
	return fixed;
}
