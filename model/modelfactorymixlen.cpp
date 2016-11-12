/*
 * modelfactorymixlen.cpp
 *
 *  Created on: Sep 2, 2015
 *      Author: minh
 */

#include "phylotreemixlen.h"
#include "timeutil.h"
#include "tools.h"
#include "model/modelfactorymixlen.h"
#include "model/modelgtr.h"
#include "model/modelmixture.h"
#include "rateheterotachy.h"

ModelFactoryMixlen::ModelFactoryMixlen(Params &params, PhyloTree *tree, ModelsBlock *models_block) :   
    ModelFactory(params, tree, models_block) {
    if (!tree->isMixlen()) {
        cerr << "ERROR: Please add '-mixlen " << site_rate->getNRate() << "' option into the command line" << endl;
        outError("Sorry for the inconvience, please rerun IQ-TREE with option above");
    }
    if (tree->getMixlen() != site_rate->getNRate()) {
        ((PhyloTreeMixlen*)tree)->setMixlen(site_rate->getNRate());
//        outError("#heterotachy classes and #mixture branch lengths do not match");
    }
    if (fused_mix_rate) {
        // fix the rate of heterotachy
//        RateHeterotachy *hrate = (RateHeterotachy*)site_rate;
        ModelMixture *mmodel = (ModelMixture*)model;
        if (site_rate->getFixParams() == 1) {
            // swap the weights between model and site_rate
            for (int i = 0; i < site_rate->getNRate(); i++) {
                mmodel->setMixtureWeight(i, site_rate->getProp(i));
                site_rate->setProp(i, 1.0);
            }
        } else {
            // fix the weight of heterotachy model to 1
            site_rate->setFixParams(2);
            double fix_prop = (1.0-site_rate->getPInvar());
            for (int i = 0; i < site_rate->getNRate(); i++)
                site_rate->setProp(i, fix_prop);
        }
    }
}

double ModelFactoryMixlen::optimizeParameters(int fixed_len, bool write_info, double logl_epsilon, double gradient_epsilon) {

	PhyloTreeMixlen *tree = (PhyloTreeMixlen*)site_rate->getTree();
	assert(tree);
    
    tree->initializeMixlen(logl_epsilon);

    double score = ModelFactory::optimizeParameters(fixed_len, write_info, logl_epsilon, gradient_epsilon);

    return score;
}

void ModelFactoryMixlen::sortClassesByTreeLength() {

	PhyloTreeMixlen *tree = (PhyloTreeMixlen*)site_rate->getTree();

    // now sort the classes by tree lengths
    DoubleVector brlen;
    tree->saveBranchLengths(brlen);
    assert(brlen.size() == tree->branchNum * tree->mixlen);

    // compute tree lengths
    double treelen[tree->mixlen];
    int index[tree->mixlen];
    memset(treelen, 0, sizeof(double)*tree->mixlen);
    int i, j;
    for (i = 0; i < tree->mixlen; i++)
        index[i] = i;
    for (i = 0, j = 0; i < brlen.size(); i++, j++) {
        if (j == tree->mixlen) j = 0;
        treelen[j] += brlen[i];
    }

    // sort tree lengths and reorder branch lengths
    quicksort(treelen, 0, tree->mixlen-1, index);
    bool sorted = true;
    for (j = 0; j < tree->mixlen; j++)
        if (index[j] != j) { sorted = false; break; };
    if (!sorted) {
        double score = tree->curScore;
        cout << "Reordering classes by tree lengths" << endl;
        DoubleVector sorted_brlen;
        sorted_brlen.resize(brlen.size());
        for (i = 0; i < tree->branchNum; i++)
            for (j = 0; j < tree->mixlen; j++)
                sorted_brlen[i*tree->mixlen + j] = brlen[i*tree->mixlen + index[j]];
        tree->restoreBranchLengths(sorted_brlen);

        assert(tree->mixlen == site_rate->getNRate());
        // reoder class weights
        double prop[site_rate->getNRate()];
        for (j = 0; j < site_rate->getNRate(); j++)
            prop[j] = site_rate->getProp(index[j]);
        for (j = 0; j < site_rate->getNRate(); j++)
            site_rate->setProp(j, prop[j]);

        // reorder mixture models
        if (fused_mix_rate) {
            assert(model->getNMixtures() == site_rate->getNRate());
            ModelMixture *mixmodel = (ModelMixture*)model;
            ModelGTR *models[model->getNMixtures()];
            for (j = 0; j < model->getNMixtures(); j++)
                models[j] = mixmodel->at(index[j]);
            for (j = 0; j < model->getNMixtures(); j++)
                mixmodel->at(j) = models[j];

            for (j = 0; j < site_rate->getNRate(); j++)
                prop[j] = mixmodel->getMixtureWeight(index[j]);
            for (j = 0; j < site_rate->getNRate(); j++)
                mixmodel->setMixtureWeight(j, prop[j]);

            // assigning memory for individual models
            int m = 0;
            int num_states = model->num_states;
            for (ModelMixture::iterator it = mixmodel->begin(); it != mixmodel->end(); it++, m++) {
                (*it)->setEigenvalues(&model->getEigenvalues()[m*num_states]);
                (*it)->setEigenvectors(&model->getEigenvectors()[m*num_states*num_states]);
                (*it)->setInverseEigenvectors(&model->getInverseEigenvectors()[m*num_states*num_states]);
            }
            model->decomposeRateMatrix();

//            model->writeInfo(cout);
            site_rate->writeInfo(cout);

        }

        tree->clearAllPartialLH();
        assert(fabs(score - tree->computeLikelihood()) < 0.1);
    }

    // update relative_rate
    double sum = 0.0;
    for (j = 0; j < tree->mixlen; j++)
        sum = treelen[j];
    sum = tree->mixlen/sum;
    for (j = 0; j < tree->mixlen; j++)
        tree->relative_rate->setRate(j, treelen[j]*sum);
}

int ModelFactoryMixlen::getNParameters() {
	int df = ModelFactory::getNParameters();
    df += site_rate->phylo_tree->branchNum * (site_rate->phylo_tree->getMixlen()-1);
	return df;
}