/*

Copyright (c) 2005-2016, University of Oxford.
All rights reserved.

University of Oxford means the Chancellor, Masters and Scholars of the
University of Oxford, having an administrative office at Wellington
Square, Oxford OX1 2JD, UK.

This file is part of Aboria.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are met:
 * Redistributions of source code must retain the above copyright notice,
   this list of conditions and the following disclaimer.
 * Redistributions in binary form must reproduce the above copyright notice,
   this list of conditions and the following disclaimer in the documentation
   and/or other materials provided with the distribution.
 * Neither the name of the University of Oxford nor the names of its
   contributors may be used to endorse or promote products derived from this
   software without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE
GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT
OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

*/


#ifndef NEIGHBOURS_H_
#define NEIGHBOURS_H_

#include <cxxtest/TestSuite.h>

#define LOG_LEVEL 1
#include "Aboria.h"

using namespace Aboria;


class NeighboursTest : public CxxTest::TestSuite {
public:
    void test_single_particle(void) {
        ABORIA_VARIABLE(scalar,double,"scalar")
    	typedef Particles<scalar> Test_type;
    	Test_type test;
    	Vect3d min(-1,-1,-1);
    	Vect3d max(1,1,1);
    	Vect3d periodic(true,true,true);
    	double diameter = 0.1;
    	test.init_neighbour_search(min,max,diameter,periodic);
    	Test_type::value_type p;

        set<position>(p,Vect3d(0,0,0));
    	test.push_back(p);

    	int count = 0;
    	for (auto tpl: test.get_neighbours(Vect3d(diameter/2,diameter/2,0))) {
    		count++;
    	}
    	TS_ASSERT_EQUALS(count,1);

    	auto tpl = test.get_neighbours(Vect3d(diameter/2,diameter/2,0));
    	TS_ASSERT_EQUALS(tpl.size(),1);

    	tpl = test.get_neighbours(Vect3d(2*diameter,0,0));
    	TS_ASSERT_EQUALS(tpl.size(),0);
    }

    void test_two_particles(void) {
        ABORIA_VARIABLE(scalar,double,"scalar")
    	typedef Particles<scalar> Test_type;
    	Test_type test;
    	Vect3d min(-1,-1,-1);
    	Vect3d max(1,1,1);
    	Vect3d periodic(true,true,true);
    	double diameter = 0.1;
    	test.init_neighbour_search(min,max,diameter,periodic);
    	Test_type::value_type p;

        set<position>(p,Vect3d(0,0,0));
    	test.push_back(p);

        set<position>(p,Vect3d(diameter/2,0,0));
    	test.push_back(p);

    	auto tpl = test.get_neighbours(Vect3d(1.1*diameter,0,0));
    	TS_ASSERT_EQUALS(tpl.size(),1);
    	const Test_type::value_type &pfound = std::get<0>(*tpl.begin());
    	TS_ASSERT_EQUALS(get<id>(pfound),get<id>(test[1]));

    	tpl = test.get_neighbours(Vect3d(0.9*diameter,0,0));
    	TS_ASSERT_EQUALS(tpl.size(),2);

    	tpl = test.get_neighbours(Vect3d(1.6*diameter,0,0));
    	TS_ASSERT_EQUALS(tpl.size(),0);
    }

    void test_create_particles(void) {
        ABORIA_VARIABLE(scalar,double,"scalar")
    	typedef Particles<scalar> Test_type;
    	Test_type test;
    	Vect3d min(-1,-1,-1);
    	Vect3d max(1,1,1);
    	Vect3d periodic(true,true,true);
    	double diameter = 0.1;
    	test.init_neighbour_search(min,max,diameter,periodic);
    	int count = 0;
    	test.create_particles(2,[&count,&test,&diameter](Test_type::value_type& i) {
    		auto tpl = test.get_neighbours(Vect3d(diameter/2,0,0));
    		count++;
    		if (count == 1) {
    			TS_ASSERT_EQUALS(tpl.size(),0);
    			return Vect3d(0,0,0);
    		} else {
    			TS_ASSERT_EQUALS(tpl.size(),1);
    			return Vect3d(diameter/2,0,0);
    		}

    	});

    	auto tpl = test.get_neighbours(Vect3d(1.1*diameter,0,0));
    	TS_ASSERT_EQUALS(tpl.size(),1);
    	const Test_type::value_type &pfound = std::get<0>(*tpl.begin());
    	TS_ASSERT_EQUALS(get<id>(pfound),get<id>(test[1]));

    	tpl = test.get_neighbours(Vect3d(0.9*diameter,0,0));
    	TS_ASSERT_EQUALS(tpl.size(),2);

    	tpl = test.get_neighbours(Vect3d(1.6*diameter,0,0));
    	TS_ASSERT_EQUALS(tpl.size(),0);
    }

};



#endif /* NEIGHBOURS_H_ */
