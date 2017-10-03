//--------------------------------------------------------------------------------------
// SharedOSPrimitiveTool.cpp
//
// Advanced Technology Group (ATG)
// Copyright (C) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------

#include "pch.h"
#include "SharedOSPrimitiveTool.h"
#include "PerfRun.h"
#include "Logging/FileLogger.h"

#include "XboxSpecificFiles/OSPrimitiveToolXbox.h"

using namespace DirectX;
using Microsoft::WRL::ComPtr;

void PerformTests(SharedSample *data)
{
	Sleep(1000);        // Wait a bit before starting the test run. 1 second is ample time for the render side to quiet down.
	PerfRun baseRun;
	if (data->m_noContention)
	{
		data->m_whatRunFinished = UINT32_MAX;
		for (uint32_t testRun = PerfRun::TestType::FirstTestTypeExecuted; testRun < PerfRun::TestType::LastTestTypeExecuted; testRun++)
		{
			for (uint32_t i = 0; i < PerfRun::s_MaxCore; i++)
			{
				baseRun.RunTests(i, i, static_cast<PerfRun::TestType> (testRun), data->m_noContention.load(), data->m_idleWorkers.load());
			}
			data->m_whatRunFinished = testRun;
		}
	}
	else
	{
		data->m_whatRunFinished = UINT32_MAX;
		for (uint32_t testRun = PerfRun::TestType::FirstTestTypeExecuted; testRun < PerfRun::TestType::LastTestTypeExecuted; testRun++)
		{
			for (uint32_t i = 0; i < PerfRun::s_MaxCore; i++)
			{
				for (uint32_t j = 1; j < PerfRun::s_MaxCore; j++)
				{
					baseRun.RunTests(i, (i + j) % PerfRun::s_MaxCore, static_cast<PerfRun::TestType> (testRun), data->m_noContention.load(), data->m_idleWorkers.load());
				}
			}
			data->m_whatRunFinished = testRun;
		}
	}
	data->m_finishedTestRun = true;
}

SharedSample::SharedSample(Sample* sample) :
	m_sample(sample)
	, m_cmdLineError(false)
	, m_shutdownThread(false)
	, m_finishedTestRun(false)
	, m_noContention(false)
	, m_idleWorkers(0)
	, m_whatRunFinished(UINT32_MAX)
{
}

SharedSample::~SharedSample()
{
	m_shutdownThread = true;
	m_workerThread->join();
	delete m_workerThread;
}

void SharedSample::Update(DX::StepTimer const& /*timer*/)
{
	if (m_sample->m_timer.GetFrameCount() == 3)     // let the system stabilize before actually running sample code.
	{                                               // Protection against issues with systems that initialize on D3D creation which can happen on the first frame.
		m_noContention = false;
		m_whatRunFinished = UINT32_MAX;
		m_workerThread = new std::thread(PerformTests, this);
	}
	if (m_finishedTestRun)
	{
		if (m_noContention == false)
		{
			m_workerThread->join();
			delete m_workerThread;
			m_noContention = true;
			m_finishedTestRun = false;
			m_workerThread = new std::thread(PerformTests, this);
		}
	}
}

void SharedSample::Render()
{
	RECT safeRect = SimpleMath::Viewport::ComputeTitleSafeArea(1920, 1080);

	XMFLOAT2 pos(float(safeRect.left), float(safeRect.top));

	m_sample->m_spriteBatch->Begin();

	m_sample->m_spriteBatch->Draw(m_sample->m_background.Get(), m_sample->m_deviceResources->GetOutputSize());

	if (m_sample->m_timer.GetFrameCount() > 3)
	{
		std::wstring outputString;
		LARGE_INTEGER freq;
		QueryPerformanceFrequency(&freq);
		{
			wchar_t buf[64] = {};
			swprintf(buf, 64, L"  Frequency:%llu  ", freq.QuadPart);
			outputString += buf;
		}
		m_sample->m_font->DrawString(m_sample->m_spriteBatch.get(), outputString.c_str(), pos);
		pos.y += 35;

		uint32_t numDots = (m_sample->m_timer.GetFrameCount() % 10) + 1;
		if (m_cmdLineError)
		{
			outputString = L"Error Parsing Command Line";
		}
		else if (m_finishedTestRun)
		{
			outputString = L"Finished Test Run";
		}
		else
		{
			outputString = L"Doing ";
			if (m_whatRunFinished == UINT32_MAX)
				outputString += PerfRun::ConvertTestTypeToString(PerfRun::FirstTestTypeDefined);
			else
				outputString += PerfRun::ConvertTestTypeToString(static_cast<PerfRun::TestType> (m_whatRunFinished + 1));
			if (m_noContention)
				outputString += L" no contention";
			else
				outputString += L" contention";
		}

		for (uint32_t i = 0; i < numDots; i++)
		{
			outputString += L".";
		}
		m_sample->m_font->DrawString(m_sample->m_spriteBatch.get(), outputString.c_str(), pos);
	}

	m_sample->m_spriteBatch->End();
}

std::vector<std::wstring> SharedSample::BreakCommandLine(const wchar_t *commandLineParams)
{
	const wchar_t *cur, *start;
	std::vector<std::wstring> toret;

	start = commandLineParams;
	cur = start;
	while (*cur != 0)
	{
		while (!isspace(*cur) && (*cur != 0))
			++cur;
		toret.push_back(std::wstring(start, cur - start));
		while (isspace(*cur) && (*cur != 0))
			++cur;
		start = cur;
	}
	if (start != cur)
	{
		toret.push_back(std::wstring(start, cur - start));
	}
	return toret;
}

void SharedSample::ParseCommandLine(const wchar_t *commandlineParams)
{
	std::vector<std::wstring> cmdLine;

	m_noContention = false;
	cmdLine = BreakCommandLine(commandlineParams);

	std::vector<std::wstring>::iterator iter, endIter;
	std::vector<std::wstring>::iterator firstParam, secondParam;
	endIter = cmdLine.end();
	for (iter = cmdLine.begin(); (iter != endIter) && (!m_cmdLineError); ++iter)
	{
		if (*iter == L"idleWorkers")
		{
			firstParam = iter;
			++firstParam;
			if (firstParam == endIter)
			{
				m_cmdLineError = true;
				continue;
			}
			m_idleWorkers = std::stoi(*firstParam);
			++iter;
			continue;
		}
		else
		{
			m_cmdLineError = true;
		}
	}
}