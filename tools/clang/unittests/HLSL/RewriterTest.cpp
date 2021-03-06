///////////////////////////////////////////////////////////////////////////////
//                                                                           //
// RewriterTest.cpp                                                          //
// Copyright (C) Microsoft Corporation. All rights reserved.                 //
// Licensed under the MIT license. See COPYRIGHT in the project root for     //
// full license information.                                                 //
//                                                                           //
// The following HLSL tests contain static_asserts and are not useful for    //
// the HLSL rewriter: more-operators.hlsl, object-operators.hlsl,            //
// scalar-operators-assign.hlsl, scalar-operators.hlsl, string.hlsl.         //
// They have been omitted.                                                   //
//                                                                           //
///////////////////////////////////////////////////////////////////////////////

#ifndef UNICODE
#define UNICODE
#endif

#include <memory>
#include <vector>
#include <string>
#include <cassert>
#include <sstream>
#include <algorithm>
#include <windows.h>
#include <unknwn.h>
#include "dxc/dxcapi.h"
#include <atlbase.h>
#include <atlfile.h>

#include "HLSLTestData.h"
#include "WexTestClass.h"
#include "HlslTestUtils.h"

#include "dxc/Support/Global.h"
#include "dxc/dxctools.h"
#include "dxc/Support/dxcapi.use.h"
#include "dxc/dxcapi.internal.h"

using namespace std;
using namespace hlsl_test;

class RewriterTest {
public:
  BEGIN_TEST_CLASS(RewriterTest)
    TEST_METHOD_PROPERTY(L"Priority", L"0")
  END_TEST_CLASS()

  TEST_METHOD(RunAttributes);
  TEST_METHOD(RunCppErrors);
  TEST_METHOD(RunIndexingOperator);
  TEST_METHOD(RunIntrinsicExamples);
  TEST_METHOD(RunMatrixAssignments);
  TEST_METHOD(RunMatrixSyntax);
  TEST_METHOD(RunPackReg);
  TEST_METHOD(RunScalarAssignments);
  TEST_METHOD(RunStructAssignments);
  TEST_METHOD(RunTemplateChecks);
  TEST_METHOD(RunTypemodsSyntax);
  TEST_METHOD(RunVarmodsSyntax);
  TEST_METHOD(RunVectorAssignments);
  TEST_METHOD(RunVectorSyntaxMix);
  TEST_METHOD(RunVectorSyntax);
  //TEST_METHOD(RunIncludes);  // TODO: Includes have not been implemented yet; uncomment when they have
  TEST_METHOD(RunStructMethods);
  TEST_METHOD(RunPredefines);
  TEST_METHOD(RunUTF16OneByte);
  TEST_METHOD(RunUTF16TwoByte);
  TEST_METHOD(RunUTF16ThreeByteBadChar);
  TEST_METHOD(RunUTF16ThreeByte);
  TEST_METHOD(RunNonUnicode);

  dxc::DxcDllSupport m_dllSupport;

  struct VerifyResult {
    std::string warnings; // warnings from first compilation
    std::string rewrite;  // output of rewrite
    
    bool HasSubstringInRewrite(const char* val) {
      return std::string::npos != rewrite.find(val);
    }
    bool HasSubstringInWarnings(const char* val) {
      return std::string::npos != warnings.find(val);
    }
  };

  std::string BlobToUtf8(_In_ IDxcBlob* pBlob) {
    if (pBlob == nullptr) return std::string();
    return std::string((char*)pBlob->GetBufferPointer(), pBlob->GetBufferSize());
  }

  void CreateBlobPinned(_In_bytecount_(size) LPCVOID data, SIZE_T size,
                        UINT32 codePage, _In_ IDxcBlobEncoding **ppBlob) {
    CComPtr<IDxcLibrary> library;
    IFT(m_dllSupport.CreateInstance(CLSID_DxcLibrary, &library));
    IFT(library->CreateBlobWithEncodingFromPinned((LPBYTE)data, size, codePage,
                                                  ppBlob));
  }

  VerifyResult CheckVerifies(LPCWSTR path, LPCWSTR goldPath) {   
    CComPtr<IDxcRewriter> pRewriter;
    VERIFY_SUCCEEDED(CreateRewriter(&pRewriter));
    
    CComPtr<IDxcOperationResult> pRewriteResult;
    RewriteCompareGold(path, goldPath, &pRewriteResult, pRewriter);

    VerifyResult toReturn;

    CComPtr<IDxcBlob> pResultBlob;
    VERIFY_SUCCEEDED(pRewriteResult->GetResult(&pResultBlob));
    toReturn.rewrite = BlobToUtf8(pResultBlob);

    CComPtr<IDxcBlobEncoding> pErrorsBlob;
    VERIFY_SUCCEEDED(pRewriteResult->GetErrorBuffer(&pErrorsBlob));
    toReturn.warnings = BlobToUtf8(pErrorsBlob);

    return toReturn;
  }
  
  HRESULT CreateRewriter(IDxcRewriter** pRewriter) {
    if (!m_dllSupport.IsEnabled()) {
      VERIFY_SUCCEEDED(m_dllSupport.Initialize());
    }
    return m_dllSupport.CreateInstance(CLSID_DxcRewriter, pRewriter);
  }

  VerifyResult CheckVerifiesHLSL(LPCWSTR name, LPCWSTR goldName) {
    return CheckVerifies(GetPathToHlslDataFile(name).c_str(),
                         GetPathToHlslDataFile(goldName).c_str());
  }

  struct FileWithBlob {
    CAtlFile file;
    CAtlFileMapping<char> mapping;
    CComPtr<IDxcBlobEncoding> BlobEncoding;

    FileWithBlob(dxc::DxcDllSupport &support, LPCWSTR path) {
      IFT(file.Create(path, GENERIC_READ, FILE_SHARE_READ, OPEN_EXISTING));
      IFT(mapping.MapFile(file));
      CComPtr<IDxcLibrary> library;
      IFT(support.CreateInstance(CLSID_DxcLibrary, &library));
      IFT(library->CreateBlobWithEncodingFromPinned((LPBYTE)mapping.GetData(),
                                                    mapping.GetMappingSize(),
                                                    CP_UTF8, &BlobEncoding));
    }
  };

  // Note: Previous versions of this file included a RewriteCompareRewrite method here that rewrote twice and compared  
  // to check for stable output.  It has now been replaced by a new test that checks against a gold baseline.

  void RewriteCompareGold(LPCWSTR path, LPCWSTR goldPath,
                          _COM_Outptr_ IDxcOperationResult **ppResult,
                          _In_ IDxcRewriter *rewriter) {
    // Get the source text from a file
    FileWithBlob source(m_dllSupport, path);

    const int myDefinesCount = 3;
    DxcDefine myDefines[myDefinesCount] = { 
      { L"myDefine", L"2" }, 
      { L"myDefine3", L"1994" }, 
      { L"myDefine4", nullptr }
    };

    // Run rewrite unchanged on the source code
    VERIFY_SUCCEEDED(rewriter->RewriteUnchanged(source.BlobEncoding, myDefines, myDefinesCount, ppResult));

    CComPtr<IDxcBlob> pRewriteResult;
    IFT((*ppResult)->GetResult(&pRewriteResult));
    std::string firstPass = BlobToUtf8(pRewriteResult);

    HANDLE goldHandle = CreateFileW(goldPath, GENERIC_READ, 0, 0, OPEN_EXISTING, 0, 0);
    VERIFY_ARE_NOT_EQUAL(goldHandle, INVALID_HANDLE_VALUE);
    CHandle checkedGoldHandle(goldHandle);

    DWORD gFileSize = GetFileSize(goldHandle, NULL);
    CComHeapPtr<char> gReadBuff;
    VERIFY_IS_TRUE(gReadBuff.AllocateBytes(gFileSize));
    DWORD gnumActualRead;
    VERIFY_WIN32_BOOL_SUCCEEDED(ReadFile(checkedGoldHandle, gReadBuff.m_pData,
                                         gFileSize, &gnumActualRead, NULL));
    std::string gold = std::string((LPSTR)gReadBuff, gnumActualRead);
    gold.erase(std::remove(gold.begin(), gold.end(), '\r'), gold.end());

      // Kept because useful for debugging
      //int atChar = 0;
      //int numDiffChar = 0;

      //while (atChar < result.size){
      //  char rewriteChar = (firstPass.data())[atChar];
      //  char goldChar = (gold.data())[atChar];
      //  
      //  if (rewriteChar != goldChar){
      //    numDiffChar++;
      //  }
      //  atChar++;
      //}
      
    if (firstPass.compare(gold) == 0) {
      return;
    }

    // Log things out before failing.
    std::wstring TestFileName(path);
    int index1 = TestFileName.find_last_of(L"\\");
    int index2 = TestFileName.find_last_of(L".");
    TestFileName = TestFileName.substr(index1+1, index2 - (index1+1));
        
    wchar_t TempPath[MAX_PATH];
    DWORD length = GetTempPathW(MAX_PATH, TempPath);
    VERIFY_WIN32_BOOL_SUCCEEDED(length != 0);
    
    std::wstring PrintName(TempPath);
    PrintName += TestFileName;
    PrintName += L"_rewrite_test_pass.txt";
        
    CHandle checkedWHandle(CreateNewFileForReadWrite(PrintName.data()));
    LPDWORD wnumWrite = 0;
    VERIFY_WIN32_BOOL_SUCCEEDED(WriteFile(checkedWHandle, firstPass.data(),
                                          firstPass.size(), wnumWrite, NULL));
        
    std::wstringstream ss;
    ss << L"\nMismatch occurred between rewriter output and expected "
          L"output. To see the differences, run:\n"
          L"diff " << goldPath << L" " << PrintName << L"\n";
    ::WEX::Logging::Log::Error(ss.str().c_str());
  }
};

TEST_F(RewriterTest, RunAttributes) {
    CheckVerifiesHLSL(L"rewriter\\attributes_noerr.hlsl", L"rewriter\\correct_rewrites\\attributes_gold.hlsl");
}

TEST_F(RewriterTest, RunCppErrors) {
    CheckVerifiesHLSL(L"rewriter\\cpp-errors_noerr.hlsl", L"rewriter\\correct_rewrites\\cpp-errors_gold.hlsl");
}

TEST_F(RewriterTest, RunIndexingOperator) {
    CheckVerifiesHLSL(L"rewriter\\indexing-operator_noerr.hlsl", L"rewriter\\correct_rewrites\\indexing-operator_gold.hlsl");
}

TEST_F(RewriterTest, RunIntrinsicExamples) {
    CheckVerifiesHLSL(L"rewriter\\intrinsic-examples_noerr.hlsl", L"rewriter\\correct_rewrites\\intrinsic-examples_gold.hlsl");
}

TEST_F(RewriterTest, RunMatrixAssignments) {
    CheckVerifiesHLSL(L"rewriter\\matrix-assignments_noerr.hlsl", L"rewriter\\correct_rewrites\\matrix-assignments_gold.hlsl");
}

TEST_F(RewriterTest, RunMatrixSyntax) {
    CheckVerifiesHLSL(L"rewriter\\matrix-syntax_noerr.hlsl", L"rewriter\\correct_rewrites\\matrix-syntax_gold.hlsl");
}

TEST_F(RewriterTest, RunPackReg) {
    CheckVerifiesHLSL(L"rewriter\\packreg_noerr.hlsl", L"rewriter\\correct_rewrites\\packreg_gold.hlsl");
}

TEST_F(RewriterTest, RunScalarAssignments) {
    CheckVerifiesHLSL(L"rewriter\\scalar-assignments_noerr.hlsl", L"rewriter\\correct_rewrites\\scalar-assignments_gold.hlsl");
}

TEST_F(RewriterTest, RunStructAssignments) {
    CheckVerifiesHLSL(L"rewriter\\struct-assignments_noerr.hlsl", L"rewriter\\correct_rewrites\\struct-assignments_gold.hlsl");
}

TEST_F(RewriterTest, RunTemplateChecks) {
    CheckVerifiesHLSL(L"rewriter\\template-checks_noerr.hlsl", L"rewriter\\correct_rewrites\\template-checks_gold.hlsl");
}

TEST_F(RewriterTest, RunTypemodsSyntax) {
    CheckVerifiesHLSL(L"rewriter\\typemods-syntax_noerr.hlsl", L"rewriter\\correct_rewrites\\typemods-syntax_gold.hlsl");
}

TEST_F(RewriterTest, RunVarmodsSyntax) {
    CheckVerifiesHLSL(L"rewriter\\varmods-syntax_noerr.hlsl", L"rewriter\\correct_rewrites\\varmods-syntax_gold.hlsl");
}

TEST_F(RewriterTest, RunVectorAssignments) {
    CheckVerifiesHLSL(L"rewriter\\vector-assignments_noerr.hlsl", L"rewriter\\correct_rewrites\\vector-assignments_gold.hlsl");
}

TEST_F(RewriterTest, RunVectorSyntaxMix) {
    CheckVerifiesHLSL(L"rewriter\\vector-syntax-mix_noerr.hlsl", L"rewriter\\correct_rewrites\\vector-syntax-mix_gold.hlsl");
}

TEST_F(RewriterTest, RunVectorSyntax) {
    CheckVerifiesHLSL(L"rewriter\\vector-syntax_noerr.hlsl", L"rewriter\\correct_rewrites\\vector-syntax_gold.hlsl");
}

// TODO: Includes have not been implemented yet; uncomment when they have
//TEST_F(RewriterTest, RunIncludes) {
//  CheckVerifiesHLSL(L"rewriter\\includes.hlsl", L"rewriter\\correct_rewrites\\includes_gold.hlsl");
//}

TEST_F(RewriterTest, RunStructMethods) {
  CheckVerifiesHLSL(L"rewriter\\struct-methods.hlsl", L"rewriter\\correct_rewrites\\struct-methods_gold.hlsl");
}

TEST_F(RewriterTest, RunPredefines) {
  CheckVerifiesHLSL(L"rewriter\\predefines.hlsl", L"rewriter\\correct_rewrites\\predefines_gold.hlsl");
}

static const UINT32 CP_UTF16 = 1200;

TEST_F(RewriterTest, RunUTF16OneByte) {
  CComPtr<IDxcRewriter> pRewriter;
  VERIFY_SUCCEEDED(CreateRewriter(&pRewriter));
  CComPtr<IDxcOperationResult> pRewriteResult;

  WCHAR utf16text[] = { L"\x0069\x006e\x0074\x0020\x0069\x003b" }; // "int i;"

  CComPtr<IDxcBlobEncoding> source;
  CreateBlobPinned(utf16text, sizeof(utf16text), CP_UTF16, &source);

  VERIFY_SUCCEEDED(pRewriter->RewriteUnchanged(source, 0, 0, &pRewriteResult));

  CComPtr<IDxcBlob> result;
  VERIFY_SUCCEEDED(pRewriteResult->GetResult(&result));

  VERIFY_IS_TRUE(strcmp(BlobToUtf8(result).c_str(), "// Rewrite unchanged result:\n\x69\x6e\x74\x20\x69\x3b\n") == 0); 
}

TEST_F(RewriterTest, RunUTF16TwoByte) {
  CComPtr<IDxcRewriter> pRewriter;
  VERIFY_SUCCEEDED(CreateRewriter(&pRewriter));
  CComPtr<IDxcOperationResult> pRewriteResult;

  WCHAR utf16text[] = { L"\x0069\x006e\x0074\x0020\x00ed\x00f1\x0167\x003b" }; // "int (i w/ acute)(n w/tilde)(t w/ 2 strokes);"

  CComPtr<IDxcBlobEncoding> source;
  CreateBlobPinned(utf16text, sizeof(utf16text), CP_UTF16, &source);

  VERIFY_SUCCEEDED(pRewriter->RewriteUnchanged(source, 0, 0, &pRewriteResult));

  CComPtr<IDxcBlob> result;
  VERIFY_SUCCEEDED(pRewriteResult->GetResult(&result));

  VERIFY_IS_TRUE(strcmp(BlobToUtf8(result).c_str(), "// Rewrite unchanged result:\n\x69\x6e\x74\x20\xc3\xad\xc3\xb1\xc5\xa7\x3b\n") == 0);
}

TEST_F(RewriterTest, RunUTF16ThreeByteBadChar) {
  CComPtr<IDxcRewriter> pRewriter;
  VERIFY_SUCCEEDED(CreateRewriter(&pRewriter));
  CComPtr<IDxcOperationResult> pRewriteResult;

  WCHAR utf16text[] = { L"\x0069\x006e\x0074\x0020\x0041\x2655\x265a\x003b" }; // "int A(white queen)(black king);"

  CComPtr<IDxcBlobEncoding> source;
  CreateBlobPinned(utf16text, sizeof(utf16text), CP_UTF16, &source);

  VERIFY_SUCCEEDED(pRewriter->RewriteUnchanged(source, 0, 0, &pRewriteResult));

  CComPtr<IDxcBlob> result;
  VERIFY_SUCCEEDED(pRewriteResult->GetResult(&result));

  VERIFY_IS_TRUE(strcmp(BlobToUtf8(result).c_str(), "// Rewrite unchanged result:\n\x69\x6e\x74\x20\x41\x3b\n") == 0); //"int A;" -> should remove the weird characters
}

TEST_F(RewriterTest, RunUTF16ThreeByte) {
  CComPtr<IDxcRewriter> pRewriter;
  VERIFY_SUCCEEDED(CreateRewriter(&pRewriter));
  CComPtr<IDxcOperationResult> pRewriteResult;

  WCHAR utf16text[] = { L"\x0069\x006e\x0074\x0020\x1e8b\x003b" }; // "int (x with dot above);"

  CComPtr<IDxcBlobEncoding> source;
  CreateBlobPinned(utf16text, sizeof(utf16text), CP_UTF16, &source);

  VERIFY_SUCCEEDED(pRewriter->RewriteUnchanged(source, 0, 0, &pRewriteResult));

  CComPtr<IDxcBlob> result;
  VERIFY_SUCCEEDED(pRewriteResult->GetResult(&result));

  VERIFY_IS_TRUE(strcmp(BlobToUtf8(result).c_str(), "// Rewrite unchanged result:\n\x69\x6e\x74\x20\xe1\xba\x8b\x3b\n") == 0);
}

TEST_F(RewriterTest, RunNonUnicode) {
  CComPtr<IDxcRewriter> pRewriter;
  VERIFY_SUCCEEDED(CreateRewriter(&pRewriter));
  CComPtr<IDxcOperationResult> pRewriteResult;

  char greektext[] = { "\x69\x6e\x74\x20\xe1\xe2\xe3\x3b" }; // "int (small alpha)(small beta)(small kappa);"

  CComPtr<IDxcBlobEncoding> source;
  CreateBlobPinned(greektext, sizeof(greektext), 1253, &source); // 1253 == ANSI Greek

  VERIFY_SUCCEEDED(pRewriter->RewriteUnchanged(source, 0, 0, &pRewriteResult));

  CComPtr<IDxcBlob> result;
  VERIFY_SUCCEEDED(pRewriteResult->GetResult(&result));

  VERIFY_IS_TRUE(strcmp(BlobToUtf8(result).c_str(), "// Rewrite unchanged result:\n\x69\x6e\x74\x20\xce\xb1\xce\xb2\xce\xb3\x3b\n") == 0);
}

