#include "D3D11RHI.h"

/**
 * @class FSwapGuard
 * @brief 렌더 타겟 핑퐁 스왑과 사용된 SRV의 자동 해제를 보장하는 RAII 클래스.
 *
 * 생성 시 SwapRenderTargets()를 호출하고, 소멸 시 지정된 범위의 SRV 슬롯을 자동으로 nullptr로 정리합니다.
 * Commit()이 호출되지 않고 스코프를 벗어나면(중간 return 등), SwapRenderTargets()를 다시 호출하여
 * 버퍼 상태를 원상 복구합니다.
 */
class FSwapGuard
{
public:
    /**
     * @param InRHI RHI 디바이스 포인터
     * @param SRVStartSlot 해제할 SRV의 시작 슬롯 번호
     * @param NumSRVsToClear 해제할 SRV의 개수
     */
    FSwapGuard(D3D11RHI* InRHI, uint32 SRVStartSlot, uint32 NumSRVsToClear)
        : RHI(InRHI)
        , bCommitted(false)
        , StartSlot(SRVStartSlot)
        , NumSRVs(NumSRVsToClear)
    {
        if (RHI)
        {
            // 생성 시 즉시 렌더 타겟 역할을 교환합니다.
            RHI->SwapRenderTargets();
        }
    }

    ~FSwapGuard()
    {
        if (RHI)
        {
            // 1. 스코프를 벗어날 때 '항상' 사용했던 SRV 바인딩을 해제합니다.
            // 이렇게 하면 다음 렌더링 패스에서 리소스 해저드 경고가 발생하는 것을 원천적으로 방지합니다.
            if (0 < NumSRVs)
            {
                // nullptr로 채워진 임시 배열을 만들어 한 번에 해제
                TArray<ID3D11ShaderResourceView*> NullSRVs(NumSRVs, nullptr);
                RHI->GetDeviceContext()->PSSetShaderResources(StartSlot, NumSRVs, NullSRVs.data());
            }

            // 2. 작업이 성공적으로 Commit되지 않았다면, 버퍼 스왑을 되돌립니다.
            if (!bCommitted)
            {
                RHI->SwapRenderTargets();
            }
        }
    }

    /**
     * @brief 스코프 내의 렌더링 작업이 성공적으로 완료되었음을 표시합니다.
     * 이 함수가 호출되면 소멸자에서 버퍼 스왑을 되돌리지 않습니다.
     */
    void Commit()
    {
        bCommitted = true;
    }

    // 복사 및 대입을 방지합니다.
    FSwapGuard(const FSwapGuard&) = delete;
    FSwapGuard& operator=(const FSwapGuard&) = delete;

private:
    D3D11RHI* RHI;
    bool bCommitted;
    uint32 StartSlot;
    uint32 NumSRVs;
};