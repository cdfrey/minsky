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

#include "CSVParser.h"
#include <ecolab_epilogue.h>
using namespace minsky;
using namespace std;

#include <boost/type_traits.hpp>
#include <boost/tokenizer.hpp>
#include <boost/token_functions.hpp>

typedef boost::escaped_list_separator<char> Parser;
typedef boost::tokenizer<Parser> Tokenizer;

struct NoDataColumns: public exception
{
  const char* what() const noexcept override {return "No data columns";}
};
struct DuplicateKey: public exception
{
  const char* what() const noexcept override {return "Duplicate key";}
};

namespace
{
  const size_t maxRowsToAnalyse=100;
  
  // returns first position of v such that all elements in that or later
  // positions are numerical or null
  size_t firstNumerical(const vector<string>& v)
  {
    size_t r=0;
    for (size_t i=0; i<v.size(); ++i)
      try
        {
          if (!v[i].empty())
            stod(v[i]);
        }
      catch (...)
        {
          r=i+1;
        }
    return r;
  }

  // counts number of non empty entries on a line
  size_t numEntries(const vector<string>& v)
  {
    size_t c=0;
    for (auto& x: v)
      if (!x.empty())
        c++;
    return c;
  }
  
  // returns true if all elements of v after start are empty
  bool emptyTail(const vector<string>& v, size_t start)
  {
    for (size_t i=start; i<v.size(); ++i)
      if (!v[i].empty()) return false;
    return true;
  }
}

template <class TokenizerFunction>
void DataSpec::givenTFguessRemainder(std::istream& input, const TokenizerFunction& tf)
{
    vector<size_t> starts;
    size_t nCols=0;
    string buf;
    size_t row=0;
    size_t firstEmpty=numeric_limits<size_t>::max();
    for (; getline(input, buf) && row<maxRowsToAnalyse; ++row)
      {
        boost::tokenizer<TokenizerFunction> tok(buf.begin(),buf.end(), tf);
        vector<string> line(tok.begin(), tok.end());
        starts.push_back(firstNumerical(line));
        nCols=max(nCols, line.size());
        // this is to detect if an empty header line is used to carry colAxes labels

        // treat a single item on its own as a comment
        if (numEntries(line)==1)
          commentRows.insert(unsigned(starts.size()-1));
        else
          if (starts.size()-1 < firstEmpty && starts.back()<nCols && emptyTail(line, starts.back()))
            firstEmpty=starts.size()-1;
      }
    // compute average of starts, then look for first row that drops below average
    double sum=0;
    for (unsigned long i=0; i<starts.size(); ++i) 
      if (commentRows.count(i)==0)
        sum+=starts[i];
    double av=sum/(starts.size()-commentRows.size());
    for (nRowAxes=0; 
         commentRows.count(nRowAxes) ||
           (starts.size()>nRowAxes && starts[nRowAxes]>av); 
         ++nRowAxes);
    for (size_t i=nRowAxes; i<starts.size(); ++i)
      nColAxes=max(nColAxes,starts[i]);
    // if more than 1 data column, treat the first row as an axis row
    if (nRowAxes==0 && nCols-nColAxes>1)
      nRowAxes=1;
    
    // treat single entry rows as comments
    if (!commentRows.empty() && 
        nColAxes == 1 && starts[*commentRows.rbegin()] == 1)
      {
        firstEmpty = *commentRows.rbegin();
        commentRows.erase(firstEmpty);
      }
    if (firstEmpty==nRowAxes) ++nRowAxes; // allow for possible colAxes header line
  }

void DataSpec::guessRemainder(std::istream& input, char sep)
{
  separator=sep;
  if (separator==' ')
    givenTFguessRemainder(input,boost::char_separator<char>()); //asumes merged whitespace separators
  else
    givenTFguessRemainder(input,Parser(escape,separator,quote));
}


void DataSpec::guessFromStream(std::istream& input)
{
  size_t numCommas=0, numSemicolons=0, numTabs=0;
  size_t row=0;
  string buf;
  ostringstream streamBuf;
  for (; getline(input, buf) && row<maxRowsToAnalyse; ++row, streamBuf<<buf<<endl)
    for (auto c:buf)
      switch (c)
        {
        case ',':
          numCommas++;
          break;
        case ';':
          numSemicolons++;
          break;
        case '\t':
          numTabs++;
          break;
        }

  istringstream inputCopy(streamBuf.str());
  if (numCommas>0.9*row && numCommas>numSemicolons && numCommas>numTabs)
    guessRemainder(inputCopy,',');
  else if (numSemicolons>0.9*row && numSemicolons>numTabs)
    guessRemainder(inputCopy,';');
  else if (numTabs>0.9*row)
    guessRemainder(inputCopy,'\t');
  else
    guessRemainder(inputCopy,' ');
}




void loadValueFromCSVFile(VariableValue& v, istream& input, const DataSpec& spec)
{
  Parser csvParser(spec.escape,spec.separator,spec.quote);
  string buf;
  typedef vector<string> Key;
  map<Key,double> tmpData;
  // stash label and order in which it appears in input file
  vector<map<string,size_t>> dimLabels(spec.nColAxes);
  bool tabularFormat=false;
  vector<XVector> xVector;
  vector<string> horizontalLabels;
                                  
  for (size_t row=0; getline(input, buf); ++row)
    {
      if (spec.commentRows.count(row)) continue;
      Tokenizer tok(buf.begin(), buf.end(), csvParser);
      
      if (row<spec.nRowAxes) // in header section
        {
          vector<string> parsedRow(tok.begin(), tok.end());
          if (parsedRow.size()<spec.nColAxes+spec.commentCols.size()+1) continue; // not a header row
          for (size_t i=0; i<spec.nColAxes; ++i)
            if (!spec.commentCols.count(i))
              xVector.emplace_back(parsedRow[i]);

          if (parsedRow.size()>spec.nColAxes+spec.commentCols.size()+1)
            {
              tabularFormat=true;
              horizontalLabels.assign(parsedRow.begin()+spec.nColAxes, parsedRow.end());
              xVector.emplace_back(spec.horizontalDimName);
              for (auto& i: horizontalLabels) xVector.back().push_back(i);
              dimLabels.emplace_back();
              for (size_t i=0; i<horizontalLabels.size(); ++i)
                dimLabels.back()[horizontalLabels[i]]=i;
            }
        }
      else // in data section
        {
          Key key;
          auto field=tok.begin();
          for (size_t i=0, dim=0; i<spec.nColAxes && field!=tok.end(); ++i, ++field)
            if (!spec.commentCols.count(i))
              {
                if (dim<xVector.size())
                  xVector.emplace_back("?"); // no header present
                key.push_back(*field);
                if (dimLabels[dim].emplace(*field, dimLabels[dim].size()).second)
                  xVector[dim].push_back(*field);
                dim++;
              }
                    
          if (field==tok.end())
            throw NoDataColumns();
          
          for (size_t col=0; field != tok.end(); ++field, ++col)
            {
              if (tabularFormat)
                key.push_back(horizontalLabels[col]);
              if (tmpData.count(key))
                throw DuplicateKey();
              try
                {
                  tmpData[key]=stod(*field);
                }
              catch (...)
                {
                  tmpData[key]=spec.missingValue;
                }
              if (tabularFormat)
                key.pop_back();
            }
        }
    }
  
  v.setXVector(xVector);
  // stash the data into vv tensorInit field
  v.tensorInit.data.clear();
  v.tensorInit.data.resize(v.numElements(), spec.missingValue);
  auto dims=v.dims();
  for (auto& i: tmpData)
    {
      size_t idx=0;
      assert (dims.size()==i.first.size());
      assert(dimLabels.size()==dims.size());
      for (size_t j=0; j<dims.size(); ++j)
        {
          assert(dimLabels[j].count(i.first[j]));
          idx = (idx*dims[j]) + dimLabels[j][i.first[j]];
        }
      v.tensorInit.data[idx]=i.second;
    }
}
