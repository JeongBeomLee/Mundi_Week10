#include "pch.h"

#include "CoroutineScheduler.h"

void UCoroutineScheduler::Start(sol::function F)
{
    sol::thread NewThread = sol::thread::create(F.lua_state());
    sol::coroutine Co(NewThread.state(), F);

    CoroutineEntry E{ NewThread, Co, CurrentTime, nullptr, false, false };
    Entries.Add(std::move(E));

    // UE_LOG("[Coroutine] Started new coroutine. Total entries: %zu", Entries.Num());
}

void UCoroutineScheduler::Update(double Dt)
{
    CurrentTime += Dt;

    if (Entries.empty())
    {
		return;
    }

    for (auto It = Entries.begin(); It != Entries.end();)
    {
        if (It->bFinished || GWorld->bPie == false)
        {
            It = Entries.erase(It);
            continue;
        }
        ++It;
    }

    for (uint32 index = 0; index < Entries.Num(); ++index)
    {
        CoroutineEntry& Entry = Entries[index];

        if(Entry.bFinished)
        {
            continue;
		}

        bool Ready = false;

        if (Entry.WaitingNextFrame)
        {
            Ready = true;
            Entry.WaitingNextFrame = false;
        }
        else if (Entry.WaitUntil)
        {
            if (Entry.WaitUntil())
            {
                Ready = true;
                Entry.WaitUntil = nullptr;
            }
        }
        else if (CurrentTime >= Entry.WakeTime)
        {

            Ready = true;
        }

        if (!Ready) { continue; }


		// auto로 받아서 sol2가 자동으로 타입 추론하도록 함
        // DeltaTime을 코루틴에 전달
        auto result = Entry.Co(Dt); // 다음 yield에 도달할 때까지 코루틴 실행 -> yield의 인자 리턴

        // 에러 체크
        if (!result.valid())
        {
            sol::error err = result;
            UE_LOG("[Coroutine Error] %s", err.what());
            Entry.bFinished = true;
            continue;
        }

        // 코루틴 상태 확인
        if (Entry.Co.status() == sol::call_status::ok)
        {
			Entry.bFinished = true;
            continue;
        }

        // yield 값 해석 - sol::object로 명시적 변환
        sol::object YieldedValue = result;  // auto -> sol::object
        sol::type ValueType = YieldedValue.get_type();


        // nil 체크
        if (ValueType == sol::type::lua_nil)
        {
            Entry.WaitingNextFrame = true;
        }
        // number 타입
        else if (ValueType == sol::type::number)
        {
            double Sec = YieldedValue.as<double>();
            Entry.WakeTime = CurrentTime + Sec;
        }
        // function 타입
        else if (ValueType == sol::type::function)
        {
            sol::function Pred = YieldedValue.as<sol::function>();
            Entry.WaitUntil = [Pred]() {
                sol::protected_function_result R = Pred();
                return R.valid() && R.get<bool>();
            };
        }
        else
        {
            Entry.WaitingNextFrame = true;
        }
    }
}
