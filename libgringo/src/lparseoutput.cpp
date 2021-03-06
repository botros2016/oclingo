// Copyright (c) 2009, Roland Kaminski <kaminski@cs.uni-potsdam.de>
//
// This file is part of gringo.
//
// gringo is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// gringo is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with gringo.  If not, see <http://www.gnu.org/licenses/>.

#include <gringo/lparseoutput.h>
#include <gringo/domain.h>
#include <gringo/storage.h>

LparseOutput::LparseOutput(std::ostream &out, bool shiftDisj)
	: LparseConverter(shiftDisj)
	, out_(out)
	, symbols_(1)
	, hasExternal_(false)
{
}

void LparseOutput::printBasicRule(uint32_t head, const AtomVec &pos, const AtomVec &neg)
{
	out_ << "1 " << head << " " << pos.size() + neg.size() << " " << neg.size();
	foreach(uint32_t lit, neg) out_ << " " << lit;
	foreach(uint32_t lit, pos) out_ << " " << lit;
	out_ << "\n";
}

void LparseOutput::printConstraintRule(uint32_t head, int32_t bound, const AtomVec &pos, const AtomVec &neg)
{
	out_ << 2 << " " << head << " " << (pos.size() + neg.size()) << " " << neg.size() << " " << bound;
	foreach(uint32_t lit, neg) out_ << " " << lit;
	foreach(uint32_t lit, pos) out_ << " " << lit;
	out_ << "\n";
}

void LparseOutput::printChoiceRule(const AtomVec &head, const AtomVec &pos, const AtomVec &neg)
{
	out_ << "3 " << head.size();
	foreach(uint32_t lit, head) out_ << " " << lit;
	out_ << " " << pos.size() + neg.size() << " " << neg.size();
	foreach(uint32_t lit, neg) out_ << " " << lit;
	foreach(uint32_t lit, pos) out_ << " " << lit;
	out_ << "\n";
}

void LparseOutput::printWeightRule(uint32_t head, int32_t bound, const AtomVec &pos, const AtomVec &neg, const WeightVec &wPos, const WeightVec &wNeg)
{
	out_ << 5 << " " << head << " " << bound << " " << (pos.size() + neg.size()) << " " << neg.size();
	foreach(uint32_t lit, neg) out_ << " " << lit;
	foreach(uint32_t lit, pos) out_ << " " << lit;
	foreach(int32_t weight, wNeg) out_ << " " << weight;
	foreach(int32_t weight, wPos) out_ << " " << weight;
	out_ << "\n";
}

void LparseOutput::printMinimizeRule(const AtomVec &pos, const AtomVec &neg, const WeightVec &wPos, const WeightVec &wNeg)
{
	out_ << 6 << " " << 0 << " " << (pos.size() + neg.size()) << " " << neg.size();
	foreach(uint32_t lit, neg) out_ << " " << lit;
	foreach(uint32_t lit, pos) out_ << " " << lit;
	foreach(int32_t weight, wNeg) out_ << " " << weight;
	foreach(int32_t weight, wPos) out_ << " " << weight;
	out_ << "\n";
}

void LparseOutput::printDisjunctiveRule(const AtomVec &head, const AtomVec &pos, const AtomVec &neg)
{
	out_ << 8 << " " << head.size();
	foreach(uint32_t lit, head) out_ << " " << lit;
	out_ << " " << (pos.size() + neg.size()) << " " << neg.size();
	foreach(uint32_t lit, neg) out_ << " " << lit;
	foreach(uint32_t lit, pos) out_ << " " << lit;
	out_ << "\n";
}

void LparseOutput::printComputeRule(int models, const AtomVec &pos, const AtomVec &neg)
{
	(void)models;
	(void)pos;
	(void)neg;
}

void LparseOutput::printSymbolTableEntry(uint32_t symbol, const std::string &name)
{
	out_ << symbol << " " << name << "\n";
}

void LparseOutput::printExternalTableEntry(const Symbol &symbol)
{
	if(!hasExternal_)
	{
		out_ << "E\n";
		hasExternal_ = true;
	}
	out_ << symbol.symbol << " ";
	symbol.print(storage(), out_);
	out_ << "\n";
}

uint32_t LparseOutput::symbol()
{
	return symbols_++;
}

void LparseOutput::doFinalize()
{
	out_ << "0\n";
	printSymbolTable();
	out_ << "0\nB+\n";
	for(size_t i = 0; i < computePos_.size(); i++) out_ << computePos_[i] << "\n";
	out_ << "0\nB-\n" << false_ << "\n";
	for(size_t i = 0; i < computeNeg_.size(); i++) out_ << computeNeg_[i] << "\n";
	out_ << "0\n";
	printExternalTable();
	if(hasExternal_) out_ << "0\n";
	out_ << "1\n";
}

