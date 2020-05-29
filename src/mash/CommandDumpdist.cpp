//covert binary dist output to text format
//yzk 2020/5/28

#include "CommandDumpdist.h"
#include "CommandDistance.h"
#include "Sketch.h"

#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <sys/stat.h> //FIXME: port to windows
#include <sys/time.h>

#include <stdint.h>
#include <cstdio>

using namespace::std;

int64_t getFileSize( const char * fileName)
{
	struct stat statbuf;
	stat(fileName, &statbuf);
	return (int64_t)statbuf.st_size;
}

double get_sec()
{
	struct timeval tv;
	gettimeofday(&tv, NULL);
	return (double)tv.tv_sec + (double)tv.tv_usec / 1000000;
}

namespace mash{

CommandDumpdist::CommandDumpdist() : Command()
{
	name = "dumpdist";
	summary = "convert binary dist results to text format and print results to stdout";
	description = "convert binary dist results to text format.";
	argumentString = "<reference.msh> <query.msh> <dist.bin>";
	
	addOption("output", Option(Option::String, "o", "Output", "output file", ""));

	useOption("threads");
	useOption("help");
	//addOption -o outputfile.
}

int CommandDumpdist::run() const
{
	if(arguments.size() < 3 || options.at("help").active)
	{
		print();
		return 0;
	}

	string refMsh   = arguments[0];
	string queryMsh = arguments[1];
	string fileName = arguments[2];
	string oFileName = options.at("output").argument;

	if(oFileName == "") oFileName = fileName + ".dist";
	FILE * fout = fopen(oFileName.c_str(), "wb");

	if(fout == NULL){
		cerr << "can not open result file: " << oFileName << endl;
		exit(1);
	}else{
		cerr << "writting result to " << oFileName << endl;
	}

	fstream resultFile(fileName.c_str(), ios::binary | ios::in);
	if(!resultFile.is_open()){
		cerr << "fail to open " << fileName << endl;
		return 1;
	}
	
	Sketch::Parameters parameters;
	//TODO:	setup parameters 
	int threads = options.at("threads").getArgumentAsNumber();

	Sketch querySketch;
	Sketch refSketch;

	vector<string> queryFiles;
	vector<string> refFiles;

	queryFiles.push_back(queryMsh);
	refFiles.push_back(refMsh);
	
	querySketch.initFromFiles(queryFiles, parameters);
	refSketch.initFromFiles(refFiles, parameters);

	int64_t binSize = getFileSize(fileName.c_str());
	int64_t resSize = binSize / sizeof(CommandDistance::Result);
	if(binSize % sizeof(CommandDistance::Result) != 0)
	{
		cerr << "imcomplete binary file" << endl;
		exit(1);
	}

	cerr << "ref sketches: "   << refSketch.getReferenceCount()   << endl;
	cerr << "query sketches: " << querySketch.getReferenceCount() << endl;
	cerr << "binary file size: " << binSize << endl;
	cerr << "number of results: " << resSize << endl;

	CommandDistance::Result *buffer = new CommandDistance::Result[binSize / sizeof(CommandDistance::Result)];

	resultFile.read((char*)buffer, binSize); //FIXME: portable but not memory efficient
	
	string oFilePrefix = fileName + ".dist";
#pragma omp parallel for default(shared) num_threads(threads)
	for(int i = 0; i < threads; i++)
	{
		fstream oFile(oFilePrefix + to_string(i), ios::out | ios::binary | ios::trunc);
		int64_t start = i * ((resSize + threads - 1) / threads);
		int64_t end = resSize < (i + 1) * ((resSize + threads - 1) / threads)
				    ? resSize : (i + 1) * ((resSize + threads - 1) / threads);

		for(int j = start; j < end; j++){
			//ostringstream tmp;
			//tmp
		    // << refSketch.getReference(buffer[j].refID).name << "\t" 
			// << querySketch.getReference(buffer[j].queryID).name << "\t" 
			// << buffer[j].distance << "\t" 
			// << buffer[j].pValue << "\t"
		    // << buffer[j].number << "/" << buffer[j].denom << endl;

			//oFile.write(tmp.str().c_str(), tmp.str().size());
			string tmp =
  	    	       refSketch.getReference(buffer[j].refID).name + "\t" 
				 + querySketch.getReference(buffer[j].queryID).name + "\t" 
				 + to_string(buffer[j].distance) + "\t" 
				 + to_string(buffer[j].pValue  ) + "\t"
			     + to_string(buffer[j].number  ) + "/" 
				 + to_string(buffer[j].denom   ) + "\n";// endl;
			oFile.write(tmp.c_str(), tmp.size());
		}
		oFile.close();
	}
	//while( resultFile.peek() != EOF )
	//{
	//	resultFile.read((char*)&buffer, sizeof(CommandDistance::Result));
	//	cout 
	//	     << refSketch.getReference(buffer.refID).name << "\t" 
	//		 << querySketch.getReference(buffer.queryID).name << "\t" 
	//		 //<< buffer.refID   << "\t"
	//		 //<< buffer.queryID << "\t"
	//		 << buffer.distance << "\t" 
	//		 << buffer.pValue << "\t"
	//	     << buffer.number << "/" << buffer.denom << endl;
	//}

	//combine and remove tmp files
	double t1 = get_sec();

	int tmpSize = 1<<20;
	unsigned char *tmpBuffer = new unsigned char[tmpSize];

	for(int i = 0; i < threads; i++)
	{
		FILE *tmpFile =  fopen((oFilePrefix + to_string(i)).c_str(), "rb");
		if(tmpFile == NULL)
		{
			cerr << "can not open " << (oFilePrefix + to_string(i)) << endl;
			exit(1);
		}
		int n;
		while(true)
		{
			n = fread(tmpBuffer, sizeof(unsigned char), tmpSize, tmpFile);
			fwrite((void*)tmpBuffer, sizeof(unsigned char), n, fout);
			if( n < tmpSize ) break;
		}

		fclose(tmpFile);
		remove((oFilePrefix + to_string(i)).c_str());
	}

	double t2 = get_sec();
	cerr << "combine time: " << t2 - t1 << endl;

	resultFile.close();
	fclose(fout);

	delete tmpBuffer;
	delete buffer;

	return 0;
}

} //namespace mash
