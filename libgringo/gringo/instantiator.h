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

#pragma once

#include <gringo/gringo.h>

class Instantiator
{
public:
	typedef boost::function1<bool, Grounder*> GroundedCallback;
private:
	typedef std::vector<int> BoolVec;
public:
	Instantiator(const VarVec &vars, const GroundedCallback &grounded);
	void append(Index *i);
	void fix();
	bool ground(Grounder *g);
	void reset();
	void finish();
	bool init(Grounder *g);
	void callback(const GroundedCallback &grounded);
	~Instantiator();
private:
	VarVec           vars_;
	GroundedCallback grounded_;
	IndexPtrVec      indices_;
	BoolVec          new_;
};
   
inline Instantiator* new_clone(const Instantiator&)
{
	return 0;
}

