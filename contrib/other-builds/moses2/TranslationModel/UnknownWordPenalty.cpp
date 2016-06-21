/*
 * UnknownWordPenalty.cpp
 *
 *  Created on: 28 Oct 2015
 *      Author: hieu
 */
#include <boost/foreach.hpp>
#include "UnknownWordPenalty.h"
#include "../System.h"
#include "../Scores.h"
#include "../InputType.h"
#include "../PhraseBased/Manager.h"
#include "../PhraseBased/TargetPhraseImpl.h"
#include "../PhraseBased/InputPath.h"
#include "../PhraseBased/TargetPhrases.h"
#include "../PhraseBased/Sentence.h"
#include "../SCFG/InputPath.h"
#include "../SCFG/TargetPhraseImpl.h"
#include "../SCFG/Manager.h"
#include "../SCFG/Sentence.h"

using namespace std;

namespace Moses2
{

UnknownWordPenalty::UnknownWordPenalty(size_t startInd, const std::string &line) :
    PhraseTable(startInd, line)
{
  ReadParameters();
}

UnknownWordPenalty::~UnknownWordPenalty()
{
  // TODO Auto-generated destructor stub
}

void UnknownWordPenalty::ProcessXML(
		const Manager &mgr,
		MemPool &pool,
		const Sentence &sentence,
		InputPaths &inputPaths) const
{
	const std::vector<const InputType::XMLOption*> &xmlOptions = sentence.GetXMLOptions();
	BOOST_FOREACH(const InputType::XMLOption *xmlOption, xmlOptions) {
		TargetPhraseImpl *target = TargetPhraseImpl::CreateFromString(pool, *this, mgr.system, xmlOption->translation);

	      if (xmlOption->prob) {
		      Scores &scores = target->GetScores();
	    	  scores.PlusEquals(mgr.system, *this, Moses2::TransformScore(xmlOption->prob));
	      }

	      InputPath *path = inputPaths.GetMatrix().GetValue(xmlOption->startPos, xmlOption->phraseSize - 1);
	      const SubPhrase<Moses2::Word> &source = path->subPhrase;

	      mgr.system.featureFunctions.EvaluateInIsolation(pool, mgr.system, source, *target);

	      TargetPhrases *tps = new (pool.Allocate<TargetPhrases>()) TargetPhrases(pool, 1);

	      tps->AddTargetPhrase(*target);
	      mgr.system.featureFunctions.EvaluateAfterTablePruning(pool, *tps, source);

	      path->AddTargetPhrases(*this, tps);
	}
}

void UnknownWordPenalty::Lookup(const Manager &mgr,
    InputPathsBase &inputPaths) const
{
	BOOST_FOREACH(InputPathBase *pathBase, inputPaths){
	  InputPath *path = static_cast<InputPath*>(pathBase);

	  if (SatisfyBackoff(mgr, *path)) {
		  const SubPhrase<Moses2::Word> &phrase = path->subPhrase;

		  TargetPhrases *tps = Lookup(mgr, mgr.GetPool(), *path);
		  path->AddTargetPhrases(*this, tps);
	  }
	}

}

TargetPhrases *UnknownWordPenalty::Lookup(const Manager &mgr, MemPool &pool,
    InputPath &inputPath) const
{
  const System &system = mgr.system;
  TargetPhrases *tps = NULL;

  // any other pt translate this?
  size_t numPt = mgr.system.mappings.size();
  const TargetPhrases **allTPS =
      static_cast<InputPath&>(inputPath).targetPhrases;
  for (size_t i = 0; i < numPt; ++i) {
    const TargetPhrases *otherTps = allTPS[i];

    if (otherTps && otherTps->GetSize()) {
      return tps;
    }
  }

  const SubPhrase<Moses2::Word> &source = inputPath.subPhrase;
  const Moses2::Word &sourceWord = source[0];
  const Factor *factor = sourceWord[0];

  tps = new (pool.Allocate<TargetPhrases>()) TargetPhrases(pool, 1);

  TargetPhraseImpl *target =
      new (pool.Allocate<TargetPhraseImpl>()) TargetPhraseImpl(pool, *this,
          system, 1);
  Moses2::Word &word = (*target)[0];

  //FactorCollection &fc = system.vocab;
  //const Factor *factor = fc.AddFactor("SSS", false);
  word[0] = factor;

  Scores &scores = target->GetScores();
  scores.PlusEquals(mgr.system, *this, -100);

  MemPool &memPool = mgr.GetPool();
  system.featureFunctions.EvaluateInIsolation(memPool, system, source, *target);

  tps->AddTargetPhrase(*target);
  system.featureFunctions.EvaluateAfterTablePruning(memPool, *tps, source);

  return tps;
}

void UnknownWordPenalty::EvaluateInIsolation(const System &system,
    const Phrase<Moses2::Word> &source, const TargetPhrase<Moses2::Word> &targetPhrase, Scores &scores,
    SCORE *estimatedScore) const
{

}

void UnknownWordPenalty::InitActiveChart(MemPool &pool, SCFG::InputPath &path) const
{
}

void UnknownWordPenalty::Lookup(MemPool &pool,
    const SCFG::Manager &mgr,
    size_t maxChartSpan,
    const SCFG::Stacks &stacks,
    SCFG::InputPath &path) const
{
  const System &system = mgr.system;

  size_t numWords = path.range.GetNumWordsCovered();
  if (numWords > 1) {
    // only create 1 word phrases
    return;
  }

  if (path.GetNumRules()) {
    return;
  }

  // don't do 1st of last word
  if (path.range.GetStartPos() == 0) {
    return;
  }
  const SCFG::Sentence &sentence = static_cast<const SCFG::Sentence&>(mgr.GetInput());
  if (path.range.GetStartPos() + 1 == sentence.GetSize()) {
    return;
  }

  // terminal
  const SCFG::Word &lastWord = path.subPhrase.Back();
  //cerr << "UnknownWordPenalty lastWord=" << lastWord << endl;

  const Factor *factor = lastWord[0];
  SCFG::TargetPhraseImpl *tp = new (pool.Allocate<SCFG::TargetPhraseImpl>(1)) SCFG::TargetPhraseImpl(pool, *this, system, 1);
  SCFG::Word &word = (*tp)[0];
  word.CreateFromString(system.GetVocab(), system, factor->GetString().as_string());

  tp->lhs.CreateFromString(system.GetVocab(), system, "[X]");

  size_t endPos = path.range.GetEndPos();
  const SCFG::InputPath &subPhrasePath = *mgr.GetInputPaths().GetMatrix().GetValue(endPos, 1);

  SCFG::SymbolBind symbolBind(pool);
  symbolBind.Add(subPhrasePath.range, lastWord, NULL);

  Scores &scores = tp->GetScores();
  scores.PlusEquals(mgr.system, *this, -100);

  MemPool &memPool = mgr.GetPool();
  const SubPhrase<SCFG::Word> &source = path.subPhrase;
  system.featureFunctions.EvaluateInIsolation(memPool, system, source, *tp);

  path.AddTargetPhrase(pool, *this, symbolBind, tp);
}

void UnknownWordPenalty::LookupUnary(MemPool &pool,
    const SCFG::Manager &mgr,
    const SCFG::Stacks &stacks,
    SCFG::InputPath &path) const
{
}

}

