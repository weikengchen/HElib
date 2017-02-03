#include <fstream>
#include <unistd.h>

#include <NTL/ZZX.h>
#include <NTL/vector.h>

#include "FHE.h"
#include "timing.h"
#include "EncryptedArray.h"


long compareFiles(string filename1, string filename2){

  ifstream file1(filename1);
  ifstream file2(filename2);
 
  if(!file1.is_open() && !file2.is_open()){
    cerr << "Could not open one of the following files:" << endl;
    cerr << filename1 << " and/or " << filename2 << endl;
    exit(EXIT_FAILURE);
  }
 
  fstream::pos_type file1size, file2size;

  file1size = file1.seekg(0, ifstream::end).tellg();
  file1.seekg(0, ifstream::beg);

  file2size = file2.seekg(0, ifstream::end).tellg();
  file2.seekg(0, ifstream::beg);

  // Quick compare sizes.
  if(file1size != file2size){ 
    file1.close();
    file2.close();
    return -1;
  }

  const size_t BLOCKSIZE = 4096; // 4 MB

  char buffer1[BLOCKSIZE];
  char buffer2[BLOCKSIZE];
  size_t blocksize = 0; // Actual blocksize

  for(size_t i=file1size; i >= blocksize ; i-=blocksize) {

    blocksize = std::min(BLOCKSIZE, i);

    file1.read(buffer1, blocksize);
    file2.read(buffer2, blocksize);

    if(memcmp(buffer1, buffer2, blocksize)){
      return 1; // Eventually tell you which block maybe even byte!
    }

  }
  
  return 0; // Files are the same!

}


int main(int argc, char *argv[])
{
 
  cout << "\n*** TEST BINARY IO " << endl << endl;
   
  ArgMapping amap;

  long m=7;
  long r=1;
  long p=2;
  long c=2;
  long w=64;
  long L=5;
 
  amap.arg("m", m, "order of cyclotomic polynomial");
  amap.arg("p", p, "plaintext base");
  amap.arg("r", r, "lifting");
  amap.arg("c", c, "number of columns in the key-switching matrices");
  amap.arg("L", L, "number of levels wanted");
  amap.parse(argc, argv);

  const char* asciiFile1 = "iotest_ascii1.txt"; 
  const char* asciiFile2 = "iotest_ascii2.txt"; 
  const char* binFile1 = "iotest_bin.bin"; 

  
// Write ASCII and bin files.
{   

  cout << "Test to write out ASCII and Binary Files.\n";    
 
  ofstream asciiFile(asciiFile1);
  ofstream binFile(binFile1, ios::binary);
  assert(asciiFile.is_open());  

  FHEcontext* context = new FHEcontext(m, p, r);
  buildModChain(*context, L, c);  // Set the modulus chain

  context->zMStar.printout(); // Printout context params
  cout << "\tSecurity Level: " << context->securityLevel() << endl;

  FHESecKey* secKey = new FHESecKey(*context);
  FHEPubKey* pubKey = secKey;
  secKey->GenSecKey(w);
  addSome1DMatrices(*secKey);

  // ASCII 
  cout << "\tWriting ASCII1 file " << asciiFile1 << endl;
  writeContextBase(asciiFile, *context);
  asciiFile << *context << endl << endl;
  asciiFile << *pubKey << endl << endl;
  asciiFile << *secKey << endl << endl;

  // Bin
  cout << "\tWriting Binary file " << binFile1<< endl;
  writeContextBaseBinary(binFile, *context);
  writeContextBinary(binFile, *context);
  writePubKeyBinary(binFile, *pubKey);
  writeSecKeyBinary(binFile, *secKey);

  asciiFile.close();
  binFile.close();

  cout << "Test successful.\n\n";

} 

// Read in bin files and write out ASCII.
{
  cout << "Test to read binary file and write it out as ASCII" << endl;
  
  ifstream inFile(binFile1, ios::binary);
  ofstream outFile(asciiFile2);
  
  // Read in context,
  FHEcontext* context;

//  cout << "\tReading Binary Base." << endl;
  readContextBaseBinary(inFile, context);  
//  cout << "\tReading Binary Context." << endl;
  readContextBinary(inFile, *context);  

  // Read in SecKey and PubKey.
  // Got to insert pubKey into seckey obj first.
  FHESecKey* secKey = new FHESecKey(*context);
  FHEPubKey* pubKey = secKey;
  
  readPubKeyBinary(inFile, *pubKey);
  readSecKeyBinary(inFile, *secKey);

 
  // ASCII 
  cout << "\tWriting ASCII2 file." << endl;
  writeContextBase(outFile, *context);
  outFile << *context << endl << endl;
  outFile << *pubKey << endl << endl;
  outFile << *secKey << endl << endl;

  inFile.close();
  outFile.close();

  cout << "Test successful.\n\n";

}

// Compare byte-wise the two ASCII files
{
  cout << "Comparing the two ASCII files\n"; 
  
//  long differ = compareFiles("tmpfile.a.txt", "tmpfile.b.txt"); 
  long differ = compareFiles(asciiFile1, asciiFile2); 

  if(differ != 0){
    cout << "\tFAIL - Files differ. Reason: " << differ << endl;
    cout << "Test failed.\n";
    exit(EXIT_FAILURE);
  } else {
    cout << "\tSUCCESS - Files are identical.\n";
  }

  cout << "Test successful.\n\n";
}

// Read in binary and perform operation.
{

  cout << "Test reading in Binary files and performing an operation between two ctxts\n";  

  ifstream inFile(binFile1, ios::binary);

  // Read in context,
  FHEcontext* context;

//  cout << "Reading Binary Base." << endl;
  readContextBaseBinary(inFile, context);  
//  cout << "Reading Binary Context." << endl;
  readContextBinary(inFile, *context);  

  // Read in PubKey.
  FHESecKey* secKey = new FHESecKey(*context);
  FHEPubKey* pubKey = secKey;
  readPubKeyBinary(inFile, *pubKey);
  readSecKeyBinary(inFile, *secKey);
 
  // Get the ea
  const EncryptedArray& ea = *context->ea;
 
  // Setup some ptxts and ctxts.
  Ctxt c1(*pubKey), c2(*pubKey);
  NewPlaintextArray p1(ea),  p2(ea);

  random(ea, p1);
  random(ea, p2);

  ea.encrypt(c1, *pubKey, p1);
  ea.encrypt(c2, *pubKey, p2);

  // Operation multiply and add.
  mul(ea, p1, p2);
//  random(ea, p1);
  c1 *= c2;

  // Decrypt and Compare.
  NewPlaintextArray pp1(ea);
  ea.decrypt(c1, *secKey, pp1);     
	
  if(!equals(ea, p1, pp1)){
    cout << "\tpolynomials are not equal\n";
    cout << "Test failed.\n";
    exit(EXIT_FAILURE);
  }  
  
  cout << "\tpolynomials are equal\n";

  inFile.close(); 

  cout << "Test successful.\n\n";

}

  return 0;
}

