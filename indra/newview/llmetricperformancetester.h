/** 
 * @file LLMetricPerformanceTester.h 
 * @brief LLMetricPerformanceTester class definition
 *
 * $LicenseInfo:firstyear=2004&license=viewerlgpl$
 * Second Life Viewer Source Code
 * Copyright (C) 2010, Linden Research, Inc.
 * 
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation;
 * version 2.1 of the License only.
 * 
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 * 
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 * 
 * Linden Research, Inc., 945 Battery Street, San Francisco, CA  94111  USA
 * $/LicenseInfo$
 */

#ifndef LL_METRICPERFORMANCETESTER_H 
#define LL_METRICPERFORMANCETESTER_H 

class LLMetricPerformanceTester 
{
public:
	//
    //name passed to the constructor is a unique string for each tester.
    //an error is reported if the name is already used by some other tester.
    //
	LLMetricPerformanceTester(std::string name, BOOL use_default_performance_analysis) ;
	virtual ~LLMetricPerformanceTester();

	//
    //return the name of the tester
    //
	std::string getName() const { return mName ;}
	//
    //return the number of the test metrics in this tester
    //
	S32 getNumOfMetricStrings() const { return mMetricStrings.size() ;}
	//
    //return the metric string at the index
    //
	const std::string& getMetricString(U32 index) const ;

	//
    //this function to compare the test results.
    //by default, it compares the test results against the baseline one by one, item by item, 
    //in the increasing order of the LLSD label counter, starting from the first one.
	//you can define your own way to analyze performance by passing FALSE to "use_default_performance_analysis",
    //and implement two abstract virtual functions below: loadTestSession(...) and compareTestSessions(...).
    //
	void analyzePerformance(std::ofstream* os, LLSD* base, LLSD* current) ;

protected:
	//
    //insert metric strings used in the tester.
    //
	void addMetricString(std::string str) ;

	//
    //increase LLSD label by 1
    //
	void incLabel() ;
	
	//
    //the function to write a set of test results to the log LLSD.
    //
	void outputTestResults() ;

	//
    //compare the test results.
    //you can write your own to overwrite the default one.
    //
	virtual void compareTestResults(std::ofstream* os, std::string metric_string, S32 v_base, S32 v_current) ;
	virtual void compareTestResults(std::ofstream* os, std::string metric_string, F32 v_base, F32 v_current) ;

	//
	//for performance analysis use 
	//it defines an interface for the two abstract virtual functions loadTestSession(...) and compareTestSessions(...).
    //please make your own test session class derived from it.
	//
	class LLTestSession
	{
	public:
		virtual ~LLTestSession() ;
	};

	//
    //load a test session for log LLSD
    //you need to implement it only when you define your own way to analyze performance.
    //otherwise leave it empty.
    //
	virtual LLMetricPerformanceTester::LLTestSession* loadTestSession(LLSD* log) = 0 ;
	//
    //compare the base session and the target session
    //you need to implement it only when you define your own way to analyze performance.
    //otherwise leave it empty.
    //
	virtual void compareTestSessions(std::ofstream* os) = 0 ;
	//
    //the function to write a set of test results to the log LLSD.
    //you have to write you own version of this function.	
	//
	virtual void outputTestRecord(LLSD* sd) = 0 ;

private:
	void preOutputTestResults(LLSD* sd) ;
	void postOutputTestResults(LLSD* sd) ;
	void prePerformanceAnalysis() ;

protected:
	//
    //the unique name string of the tester
    //
	std::string mName ;
	//
    //the current label counter for the log LLSD
    //
	std::string mCurLabel ;
	S32 mCount ;
	
	BOOL mUseDefaultPerformanceAnalysis ;
	LLTestSession* mBaseSessionp ;
	LLTestSession* mCurrentSessionp ;

	//metrics strings
	std::vector< std::string > mMetricStrings ;

//static members
private:
	static void addTester(LLMetricPerformanceTester* tester) ;

public:	
	typedef std::map< std::string, LLMetricPerformanceTester* > name_tester_map_t;	
	static name_tester_map_t sTesterMap ;

	static LLMetricPerformanceTester* getTester(std::string label) ;
	static BOOL hasMetricPerformanceTesters() {return !sTesterMap.empty() ;}

	static void initClass() ;
	static void cleanClass() ;
};

#endif

