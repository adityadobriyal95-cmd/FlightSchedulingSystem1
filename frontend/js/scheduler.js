window.SchedulerModule = (() => {
  const COLORS = ['#0B1F3A', '#0284C7', '#059669', '#D97706', '#4F46E5', '#0D9488', '#BE123C', '#1D4ED8'];

  let customJobs = [
    { flight_number: 'AI-101', airline_name: 'Air India', arrival_time: 0, burst_time: 14, priority: 2 },
    { flight_number: 'BA-142', airline_name: 'British Airways', arrival_time: 2, burst_time: 10, priority: 1 },
    { flight_number: '6E-204', airline_name: 'IndiGo', arrival_time: 4, burst_time: 6, priority: 3 },
    { flight_number: 'FX-901', airline_name: 'FedEx Cargo', arrival_time: 5, burst_time: 12, priority: 4 }
  ];

  function renderCustomJobs() {
    const container = document.getElementById('customJobsList');
    container.innerHTML = customJobs.map((j, idx) => `
      <div class="form-grid" style="margin-bottom:8px; align-items:center;">
        <input type="text" class="form-control" value="${j.flight_number}" onchange="SchedulerModule.updateJob(${idx}, 'flight_number', this.value)" placeholder="Flight #" />
        <input type="number" class="form-control" value="${j.arrival_time}" min="0" onchange="SchedulerModule.updateJob(${idx}, 'arrival_time', Number(this.value))" placeholder="Arrival Time" />
        <input type="number" class="form-control" value="${j.burst_time}" min="1" onchange="SchedulerModule.updateJob(${idx}, 'burst_time', Number(this.value))" placeholder="Burst Time" />
        <select class="form-control" onchange="SchedulerModule.updateJob(${idx}, 'priority', Number(this.value))">
          <option value="1" ${j.priority === 1 ? 'selected' : ''}>Priority 1 (VIP/Emerg)</option>
          <option value="2" ${j.priority === 2 ? 'selected' : ''}>Priority 2 (Intl)</option>
          <option value="3" ${j.priority === 3 ? 'selected' : ''}>Priority 3 (Domestic)</option>
          <option value="4" ${j.priority === 4 ? 'selected' : ''}>Priority 4 (Cargo)</option>
        </select>
        <button type="button" class="btn btn-danger btn-sm" onclick="SchedulerModule.removeJob(${idx})">Remove</button>
      </div>
    `).join('');
  }

  function updateJob(idx, field, val) {
    customJobs[idx][field] = val;
  }

  function removeJob(idx) {
    if (customJobs.length <= 1) return;
    customJobs.splice(idx, 1);
    renderCustomJobs();
  }

  async function execute() {
    const algorithm = document.getElementById('schedAlgorithm').value;
    const source = document.getElementById('schedSource').value;
    const quantum = Number(document.getElementById('schedQuantum').value) || 8;

    try {
      const res = await apiRequest('/scheduler/run', {
        method: 'POST',
        body: JSON.stringify({
          algorithm,
          source,
          quantum,
          jobs: customJobs
        })
      });

      document.getElementById('schedulerOutputArea').style.display = 'block';
      document.getElementById('resAlgoName').textContent = res.algorithm;
      document.getElementById('resAvgWt').textContent = `${res.avg_waiting_time.toFixed(2)} min`;
      document.getElementById('resAvgTat').textContent = `${res.avg_turnaround_time.toFixed(2)} min`;
      document.getElementById('resUtil').textContent = `${res.utilization_percent.toFixed(1)}%`;

      // Render Gantt Chart
      const colorMap = new Map();
      let colorIdx = 0;
      const ganttRow = document.getElementById('ganttChartRow');
      ganttRow.innerHTML = res.gantt_chart.map(slice => {
        if (!colorMap.has(slice.flight_number)) {
          colorMap.set(slice.flight_number, COLORS[colorIdx++ % COLORS.length]);
        }
        const bg = colorMap.get(slice.flight_number);
        return `
          <div class="gantt-block" style="flex:${slice.duration}; background:${bg};"
               title="${slice.flight_number}: ${slice.start_time}m -> ${slice.end_time}m (${slice.duration}m)">
            <div>${slice.flight_number}</div>
            <span>${slice.start_time} - ${slice.end_time}m</span>
          </div>
        `;
      }).join('');

      // Render Metrics Table
      const tbody = document.getElementById('schedulerResultsBody');
      tbody.innerHTML = res.results.map(r => `
        <tr>
          <td><strong>${r.flight_number}</strong></td>
          <td>${r.airline_name}</td>
          <td><span class="badge badge-info">P${r.priority}</span></td>
          <td>${r.arrival_time} m</td>
          <td>${r.burst_time} m</td>
          <td><strong>${r.completion_time} m</strong></td>
          <td>${r.turnaround_time} m</td>
          <td>${r.waiting_time} m</td>
        </tr>
      `).join('');

      showToast(`Executed ${res.algorithm}`);
    } catch (err) {
      showToast(err.message, true);
    }
  }

  function init() {
    const algoSel = document.getElementById('schedAlgorithm');
    const sourceSel = document.getElementById('schedSource');

    algoSel.addEventListener('change', () => {
      document.getElementById('quantumGroup').style.display =
        algoSel.value === 'ROUND_ROBIN' ? 'flex' : 'none';
    });

    sourceSel.addEventListener('change', () => {
      const isCustom = sourceSel.value === 'CUSTOM';
      document.getElementById('customJobsEditor').style.display = isCustom ? 'block' : 'none';
      if (isCustom) renderCustomJobs();
    });

    document.getElementById('addCustomJobBtn').addEventListener('click', () => {
      customJobs.push({
        flight_number: `FL-${100 + customJobs.length * 11}`,
        airline_name: 'Charter Ops',
        arrival_time: customJobs.length * 2,
        burst_time: 8,
        priority: 3
      });
      renderCustomJobs();
    });

    document.getElementById('runSchedulerBtn').addEventListener('click', execute);
  }

  return { init, updateJob, removeJob };
})();
