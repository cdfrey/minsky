/*
  @copyright Steve Keen 2018
  @author Russell Standish
  This file is part of Minsky.

  Minsky is free software: you can redistribute it and/or modify it
  under the terms of the GNU General Public License as published by
  the Free Software Foundation, either version 3 of the License, or
  (at your option) any later version.

  Minsky is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with Minsky.  If not, see <http://www.gnu.org/licenses/>.
*/

#include "variableType.h"
#include "evalOp.h"
#include "selection.h"
#include "xvector.h"
#include "minskyTensorOps.h"
#include "minsky.h"
#include "minsky_epilogue.h"
#include <UnitTest++/UnitTest++.h>
using namespace minsky;

#include <exception>
using namespace std;

#include <boost/date_time.hpp>
using namespace boost;
using namespace boost::posix_time;
using namespace boost::gregorian;

// convenience structure to avoid putting items into Minsky
// note privately derived from EvalCommon pointer to get around initialisation order issues
struct Eval: private std::shared_ptr<EvalCommon>, public TensorEval
{
  template <class Op>
  Eval(VariableBase& result, Op& op):
    shared_ptr<EvalCommon>(new EvalCommon),
    TensorEval(*result.vValue(), *this,
               TensorOpFactory().create(op,TensorsFromPort(*this))) {}
  
  void operator()() {TensorEval::eval(ValueVector::flowVars.data(), ValueVector::stockVars.data());}
};

struct TestFixture
{
  Variable<VariableType::flow> from, to;
  VariableValue& fromVal;
  TestFixture(): from("from"), to("to"), fromVal(*from.vValue()) {
    fromVal.hypercube(Hypercube(vector<unsigned>{5}));
    for (auto& i: fromVal)
      i=&i-&fromVal[0]+1;
  }

  template <OperationType::Type op>
  void evalOp(const std::string& axis="", double arg=0)
  {
    Operation<op> theOp;
    theOp.axis=axis;
    theOp.arg=arg;
    Wire w1(from.ports[0], theOp.ports[1]), w2(theOp.ports[0], to.ports[1]);
    Eval(to, theOp)();
  }
  template <OperationType::Type op, class F>
  void checkReduction(F f)
  {
    // reduce all
    evalOp<op>();
    double ref=fromVal[0];
    for (size_t i=1; i<fromVal.size(); ++i) ref=f(ref, fromVal[i]);
    CHECK_EQUAL(ref,to.vValue()->value());

    vector<unsigned> dims{5,5};
    fromVal.hypercube(Hypercube(dims));
    auto& toVal=*to.vValue();
    for (auto& i: fromVal)
      i=&i-&fromVal[0]+1;

    // reduce over first dimension
    evalOp<op>("0");
    CHECK_EQUAL(1, toVal.rank());
    for (size_t i=0; i<toVal.size(); ++i)
      {
        double ref=fromVal[dims[0]*i];
        for (size_t j=1; j<dims[0]; ++j)
          ref=f(ref,fromVal[j+dims[0]*i]);
        CHECK_EQUAL(ref,toVal[i]);
      }
      
    evalOp<op>("1");
    CHECK_EQUAL(1, toVal.rank());
    for (size_t i=0; i<toVal.size(); ++i)
      {
        double ref=fromVal[i];
        for (size_t j=1; j<dims[1]; ++j)
          ref=f(ref,fromVal[i+dims[0]*j]);
        CHECK_EQUAL(ref,toVal[i]);
      }
  }
};

struct MinskyFixture: public Minsky
{
  LocalMinsky lm{*this};
};

SUITE(TensorOps)
{
  TEST_FIXTURE(TestFixture, reduction)
    {
      checkReduction<OperationType::sum>([](double x,double y){return x+y;});
      checkReduction<OperationType::product>([](double x,double y){return x*y;});
      checkReduction<OperationType::infimum>([](double x,double y){return x<y? x: y;});
      checkReduction<OperationType::supremum>([](double x,double y){return x>y? x: y;});
      checkReduction<OperationType::any>([](double x,double y){return x>0.5 || y>0.5;});
      checkReduction<OperationType::all>([](double x,double y){return x>0.5 && y>0.5;});

      fromVal[3]=0;
      checkReduction<OperationType::all>([](double x,double y){return x>0.5 && y>0.5;});

      for (auto& i: fromVal)
        i=0;
      checkReduction<OperationType::any>([](double x,double y){return x>0.5 || y>0.5;});
      checkReduction<OperationType::all>([](double x,double y){return x>0.5 && y>0.5;});

      fromVal[1]=-1;
      fromVal[3]=100;
      evalOp<OperationType::infIndex>();
      CHECK_EQUAL(1,to.vValue()->value());
      evalOp<OperationType::supIndex>();
      CHECK_EQUAL(3,to.vValue()->value());
    }
  
  TEST_FIXTURE(TestFixture, scan)
    {
      for (auto& i: fromVal)
        i=2;
      
      evalOp<OperationType::runningSum>();
      {
        auto& toVal=*to.vValue();
        for (size_t i=0; i<toVal.size(); ++i)
          CHECK_EQUAL(2*(i+1),toVal[i]);
      }
      
      evalOp<OperationType::runningProduct>();
      {
        auto& toVal=*to.vValue();
        for (size_t i=0; i<toVal.size(); ++i)
          CHECK_EQUAL(pow(2,i+1),toVal[i]);
      }

      vector<unsigned> dims{5,5};
      fromVal.hypercube(Hypercube(dims));
      for (size_t i=0; i<dims[0]; ++i)
        for (size_t j=0; j<dims[1]; ++j)
          fromVal[i+j*dims[0]] = i+j; 

      int bin=2;
      evalOp<OperationType::runningSum>("0",2);
      {
        auto& toVal=*to.vValue();
        for (size_t i=0; i<dims[0]; ++i)
          for (size_t j=0; j<dims[1]; ++j)
            {
              double ref=0;
              for (int k=max(int(i)-bin,0); k<=i; ++k)
                ref+=fromVal[k+j*dims[0]];
              cout << i<<","<<j<<endl;
              CHECK_EQUAL(ref,toVal[i+j*dims[0]]);
            }
      }
      evalOp<OperationType::runningSum>("1",2);
      {
        auto& toVal=*to.vValue();
        for (size_t i=0; i<toVal.size(); ++i)
          CHECK_EQUAL(2*(i+1),toVal[i]);
      }
     
      evalOp<OperationType::runningProduct>("0",2);
      {
        auto& toVal=*to.vValue();
        for (size_t i=0; i<toVal.size(); ++i)
          CHECK_EQUAL(pow(2,i+1),toVal[i]);
      }
      evalOp<OperationType::runningProduct>("1",2);
      {
        auto& toVal=*to.vValue();
        for (size_t i=0; i<toVal.size(); ++i)
          CHECK_EQUAL(pow(2,i+1),toVal[i]);
      }
    }

  TEST_FIXTURE(TestFixture, indexGather)
    {
      auto& toVal=*to.vValue();
      for (auto& i: fromVal)
        i=(&i-&fromVal[0])%2;
      evalOp<OperationType::index>();
      vector<double> expected{1,3};
      CHECK_ARRAY_EQUAL(expected,toVal.begin(),2);
      for (size_t i=3; i<toVal.size(); ++i)
        CHECK(std::isnan(toVal[i]));

      // apply gather to the orignal vector and the index results.
      Operation<OperationType::gather> gatherOp;
      Variable<VariableType::flow> gatheredVar("gathered");
      Wire w1(from.ports[0], gatherOp.ports[1]);
      Wire w2(to.ports[0], gatherOp.ports[2]);
      Wire w3(gatherOp.ports[0], gatheredVar.ports[1]);

      auto& gathered=*gatheredVar.vValue();
      Eval eval(gatheredVar, gatherOp);
      eval();
      
      // replace nans with -1 to make comparison test simpler
      for (auto& g: gathered)
        if (!finite(g)) g=-1;
      expected={1,1,-1,-1,-1};
      CHECK_ARRAY_EQUAL(expected,gathered.begin(),5);

      // another example - check for corner cases
      vector<double> data{0.36,0.412,0.877,0.437,0.751};
      memcpy(fromVal.begin(),&data[0],data.size()*sizeof(data[0]));
      
      eval();
      expected={1,3};
      CHECK_ARRAY_EQUAL(expected,toVal.begin(),2);
      for (size_t i=3; i<toVal.size(); ++i)
        CHECK(std::isnan(toVal[i]));

      // replace nans with -1 to make comparison test simpler
      for (auto& g: gathered)
        if (!finite(g)) g=-1;
      expected={0.412, 0.437, -1, -1, -1};
      CHECK_ARRAY_EQUAL(expected,gathered.begin(),5);
    }

  TEST_FIXTURE(MinskyFixture, tensorUnOpFactory)
    {
      TensorOpFactory factory;
      auto ev=make_shared<EvalCommon>();
      TensorsFromPort tp(ev);
      Variable<VariableType::flow> src("src"), dest("dest");
      src.init("iota(5)");
      variableValues.reset();
      CHECK_EQUAL(1,src.vValue()->rank());
      CHECK_EQUAL(5,src.vValue()->size());
      for (OperationType::Type op=OperationType::copy; op<OperationType::innerProduct;
           op=OperationType::Type(op+1))
        {
          OperationPtr o(op);
          CHECK_EQUAL(2, o->numPorts());
          Wire w1(src.ports[0], o->ports[1]), w2(o->ports[0], dest.ports[1]);
          TensorEval eval(*dest.vValue(), ev, factory.create(*o,tp));
          eval.eval(ValueVector::flowVars.data(), ValueVector::stockVars.data());
          switch (OperationType::classify(op))
            {
            case OperationType::function:
              {
                // just check that scalar functions are performed elementwise
                CHECK_EQUAL(src.vValue()->size(), dest.vValue()->size());
                unique_ptr<ScalarEvalOp> scalarOp(ScalarEvalOp::create(op));
                CHECK(scalarOp.get());
                for (size_t i=0; i<src.vValue()->size(); ++i)
                  {
                    double x=scalarOp->evaluate((*src.vValue())[i]);
                    double y=(*dest.vValue())[i];
                    if (finite(x)||finite(y))
                      CHECK_EQUAL(x,y);
                  }
                break;
              }
            case OperationType::reduction:
              CHECK_EQUAL(0, dest.vValue()->rank());
              CHECK_EQUAL(1, dest.vValue()->size());
              break;
            case OperationType::scan:
              CHECK_EQUAL(1, dest.vValue()->rank());
              CHECK_EQUAL(src.vValue()->size(), dest.vValue()->size());
              break;
            default:
              CHECK(false);
              break;
            }
        }
    }
  
  TEST_FIXTURE(MinskyFixture, tensorBinOpFactory)
    {
      TensorOpFactory factory;
      auto ev=make_shared<EvalCommon>();
      TensorsFromPort tp(ev);
      Variable<VariableType::flow> src1("src1"), src2("src2"), dest("dest");
      src1.init("iota(5)");
      src2.init("one(5)");
      variableValues.reset();
      CHECK_EQUAL(1,src1.vValue()->rank());
      CHECK_EQUAL(5,src1.vValue()->size());
      CHECK_EQUAL(1,src2.vValue()->rank());
      CHECK_EQUAL(5,src2.vValue()->size());
      for (OperationType::Type op=OperationType::add; op<OperationType::copy;
           op=OperationType::Type(op+1))
        {
          OperationPtr o(op);
          CHECK_EQUAL(3, o->numPorts());
          Wire w1(src1.ports[0], o->ports[1]), w2(src2.ports[0], o->ports[2]),
            w3(o->ports[0], dest.ports[1]);
          TensorEval eval(*dest.vValue(), ev, factory.create(*o,tp));
          eval.eval(ValueVector::flowVars.data(), ValueVector::stockVars.data());
          CHECK_EQUAL(src1.vValue()->size(), dest.vValue()->size());
          CHECK_EQUAL(src2.vValue()->size(), dest.vValue()->size());
          unique_ptr<ScalarEvalOp> scalarOp(ScalarEvalOp::create(op));
          for (size_t i=0; i<src1.vValue()->size(); ++i)
            {
              double x=scalarOp->evaluate((*src1.vValue())[i], (*src2.vValue())[i]);
              double y=(*dest.vValue())[i];
              if (finite(x)||finite(y))
                CHECK_EQUAL(x,y);
            }
        }
    }

  template <OperationType::Type op, class F, class F2>
    void multiWireTest(double identity, F f, F2 f2)
  {
    //cout << OperationType::typeName(op)<<endl;
    Operation<op> o;
    auto tensorOp=TensorOpFactory().create(o);
    CHECK_EQUAL(1, tensorOp->size());
    CHECK_EQUAL(identity, (*tensorOp)[0]);
    Hypercube hc(vector<unsigned>{2});
    auto tv1=make_shared<TensorVal>(hc), tv2=make_shared<TensorVal>(hc);
    tv1->push_back(0,1), tv2->push_back(0,2);
    tv1->push_back(1,2), tv2->push_back(1,1);
    tensorOp->setArguments(vector<TensorPtr>{tv1,tv2},vector<TensorPtr>{});
    CHECK_EQUAL(f((*tv1)[0],(*tv2)[0]), (*tensorOp)[0]);
    CHECK_EQUAL(f((*tv1)[1],(*tv2)[1]), (*tensorOp)[1]);
    tensorOp->setArguments(vector<TensorPtr>{},vector<TensorPtr>{tv1,tv2});
    CHECK_EQUAL(f2(f((*tv1)[0],(*tv2)[0])), (*tensorOp)[0]);
    CHECK_EQUAL(f2(f((*tv1)[1],(*tv2)[1])), (*tensorOp)[1]);
  }

  TEST_FIXTURE(MinskyFixture, tensorBinOpMultiples)
    {
      auto id=[](double x){return x;};
      multiWireTest<OperationType::add>(0, [](double x,double y){return x+y;},id);
      multiWireTest<OperationType::subtract>
        (0, [](double x,double y){return x+y;}, [](double x){return -x;});
      multiWireTest<OperationType::multiply>(1, [](double x,double y){return x*y;}, id);
      multiWireTest<OperationType::divide>
        (1, [](double x,double y){return x*y;}, [](double x){return 1/x;});
      multiWireTest<OperationType::min>
        (std::numeric_limits<double>::max(), [](double x,double y){return x<y? x: y;}, id);
      multiWireTest<OperationType::max>
        (-std::numeric_limits<double>::max(), [](double x,double y){return x>y? x: y;}, id);
      multiWireTest<OperationType::and_>(1, [](double x,double y){return x>0.5 && y>0.5;}, id);
      multiWireTest<OperationType::or_>(0, [](double x,double y){return x>0.5 || y>0.5;}, id);
    }

  
}
