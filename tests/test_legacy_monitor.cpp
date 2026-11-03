#include <assert.h>
#include <stdio.h>
#include "../SmartTree/PlantMonitor.h"

static void samples(PlantMonitor& m, uint32_t start, unsigned duration, bool present) {
    for (unsigned dt = 0; dt <= duration; dt += 100) m.observe(start + dt, present);
}

int main() {
    {
        PlantMonitor m;
        samples(m, 0, 1900, true);
        assert(m.state() == PlantMonitor::CHECKING && m.confirms() == 0);
        assert(m.observe(2000, true) == PlantMonitor::PLANT_CONFIRMED);
        assert(m.state() == PlantMonitor::CONFIRMED && m.confirms() == 1);
        samples(m, 2100, 60000, true);
        assert(m.confirms() == 1 && m.alerts() == 0);
    }
    {
        PlantMonitor m;
        samples(m, 0, 4900, false);
        assert(m.state() == PlantMonitor::CHECKING && m.alerts() == 0);
        assert(m.observe(5000, false) == PlantMonitor::PLANT_UNCONFIRMED);
        samples(m, 5100, 60000, false);
        assert(m.state() == PlantMonitor::UNCONFIRMED && m.alerts() == 1);
        samples(m, 65200, 2000, true);
        assert(m.state() == PlantMonitor::CONFIRMED && m.confirms() == 1);
    }
    {
        PlantMonitor m;
        samples(m, 0, 2000, true);
        m.observe(2100, false);
        assert(m.state() == PlantMonitor::CHECKING);
        samples(m, 2200, 2000, true);
        assert(m.state() == PlantMonitor::CONFIRMED);
        assert(m.confirms() == 1 && m.alerts() == 0); // 短暫漏判不新增事件
        samples(m, 4300, 5000, false);
        assert(m.alerts() == 1);
        samples(m, 9400, 2000, true);
        assert(m.confirms() == 2);
    }
    {
        PlantMonitor m;
        samples(m, 0, 1900, true);
        m.observe(2000, false);
        samples(m, 2100, 1900, true);
        assert(m.state() == PlantMonitor::CHECKING && m.confirms() == 0);
        m.observe(4100, true);
        assert(m.confirms() == 1); // 中途漏判會重新累积 2 秒
    }
    {
        PlantMonitor m;
        samples(m, 0, 2000, true);
        assert(m.tick(3499) == PlantMonitor::NONE);
        assert(m.tick(3500) == PlantMonitor::INFERENCE_TIMEOUT);
        assert(m.state() == PlantMonitor::NN_TIMEOUT);
        assert(m.tick(9000) == PlantMonitor::NONE); // 逾時不重複通知
        samples(m, 9100, 1900, true);
        assert(m.state() == PlantMonitor::CHECKING);
        m.observe(11100, true);
        assert(m.state() == PlantMonitor::CONFIRMED && m.confirms() == 2);
    }
    {
        PlantMonitor m;
        m.reset(1000);
        assert(m.tick(2499) == PlantMonitor::NONE);
        assert(m.tick(2500) == PlantMonitor::INFERENCE_TIMEOUT);
        assert(m.alerts() == 0); // 完全沒新資料不是「沒盆栽」
    }
    {
        PlantMonitor m;
        samples(m, 0, 2000, true);
        m.offline();
        assert(m.state() == PlantMonitor::OFFLINE && m.tick(9000) == PlantMonitor::NONE);
        m.reconnect(10000);
        samples(m, 10000, 1900, true);
        assert(m.state() == PlantMonitor::CHECKING && m.confirms() == 1);
        m.observe(12000, true);
        assert(m.confirms() == 2);
    }
    {
        PlantMonitor m;
        samples(m, 0, 1900, true);
        m.observe(2000, true, true); // queue 遺失樣本會打斷連續判斷
        assert(m.state() == PlantMonitor::CHECKING && m.confirms() == 0);
        samples(m, 2100, 1900, true);
        assert(m.confirms() == 1);
    }
    {
        PlantMonitor m;
        m.observe(0, true);
        m.observe(2000, true); // 超長推論間隔不能假裝持續辨識
        assert(m.state() == PlantMonitor::CHECKING && m.confirms() == 0);
    }
    {
        PlantMonitor m;
        const uint32_t start = UINT32_MAX - 1000;
        m.reset(start);
        samples(m, start, 2000, true);
        assert(m.state() == PlantMonitor::CONFIRMED && m.confirms() == 1);
        assert(m.tick(start + 3500) == PlantMonitor::INFERENCE_TIMEOUT);
        m.reconnect(start + 3600);
        samples(m, start + 3600, 5000, false);
        assert(m.alerts() == 1); // millis 回繞後仍正常
    }
    puts("PASS: 10 scenario groups (timing, deduplication, recovery, timeout, reconnect, wraparound)");
}
