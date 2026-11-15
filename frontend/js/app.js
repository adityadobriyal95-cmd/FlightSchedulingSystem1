let state = {
  airports: [],
  aircraft: [],
  flights: [],
  passengers: [],
  runways: [],
  gates: [],
  report: null
};

function statusBadge(status) {
  const s = (status || '').toUpperCase();
  if (['OPERATIONAL', 'AVAILABLE', 'ACTIVE', 'CONFIRMED', 'LANDED'].includes(s)) return 'badge-success';
  if (['SCHEDULED', 'BOARDING', 'IN_AIR', 'DEPARTED'].includes(s)) return 'badge-info';
  if (['DELAYED', 'MAINTENANCE', 'RESTRICTED'].includes(s)) return 'badge-warning';
  return 'badge-danger';
}

function switchView(viewName) {
  document.querySelectorAll('.nav-item').forEach(el => {
    el.classList.toggle('active', el.dataset.view === viewName);
  });
  document.querySelectorAll('.view-section').forEach(sec => {
    sec.classList.toggle('active', sec.id === `view-${viewName}`);
  });
  const activeNav = document.querySelector(`.nav-item[data-view="${viewName}"]`);
  if (activeNav) {
    document.getElementById('pageTitle').textContent = activeNav.textContent;
  }
  document.getElementById('sidebar').classList.remove('open');
}

function populateAirportSelectors() {
  const opts = state.airports.map(a =>
    `<option value="${a.airport_id}">${a.iata_code} - ${a.city} (${a.name})</option>`
  ).join('');

  ['flOrigin', 'flDest', 'chkOrigin', 'chkDest', 'newRunwayAirport', 'newGateAirport'].forEach(id => {
    const el = document.getElementById(id);
    if (el) el.innerHTML = opts;
  });

  const acAirport = document.getElementById('acAirport');
  if (acAirport) {
    acAirport.innerHTML = `<option value="0">-- Unassigned --</option>` + opts;
  }

  const originFilter = document.getElementById('flightOriginFilter');
  if (originFilter) {
    const prev = originFilter.value;
    originFilter.innerHTML = `<option value="ALL">All Origin Airports</option>` +
      state.airports.map(a => `<option value="${a.iata_code}">${a.iata_code} - ${a.city}</option>`).join('');
    if (prev) originFilter.value = prev;
  }
}

function populateAircraftSelectors() {
  const opts = state.aircraft.map(ac =>
    `<option value="${ac.aircraft_id}">${ac.registration_no} - ${ac.model} (${ac.status})</option>`
  ).join('');
  ['flAircraft', 'chkAircraft'].forEach(id => {
    const el = document.getElementById(id);
    if (el) el.innerHTML = opts;
  });
}

function filterResourcesForAirport(airportId, gateSelectId, runwaySelectId) {
  const aid = Number(airportId);
  const gates = state.gates.filter(g => g.airport_id === aid);
  const runways = state.runways.filter(r => r.airport_id === aid);

  const gEl = document.getElementById(gateSelectId);
  const rEl = document.getElementById(runwaySelectId);
  if (gEl) {
    gEl.innerHTML = gates.map(g =>
      `<option value="${g.gate_id}">${g.terminal} - Gate ${g.gate_code} (${g.gate_type})</option>`
    ).join('');
  }
  if (rEl) {
    rEl.innerHTML = runways.map(r =>
      `<option value="${r.runway_id}">RWY ${r.runway_code} (${r.length_meters}m)</option>`
    ).join('');
  }
}

function renderDashboardAndReports() {
  const r = state.report;
  if (!r) return;

  document.getElementById('dashboardKpis').innerHTML = `
    <div class="kpi-card accent-sky">
      <span class="kpi-label">Active Flights</span>
      <div class="kpi-value">${r.total_flights - r.cancelled_flights}</div>
      <span class="kpi-sub">${r.total_flights} total scheduled</span>
    </div>
    <div class="kpi-card accent-emerald">
      <span class="kpi-label">Fleet Ready</span>
      <div class="kpi-value">${r.available_aircraft} / ${r.total_aircraft}</div>
      <span class="kpi-sub">Aircraft operational</span>
    </div>
    <div class="kpi-card">
      <span class="kpi-label">Airports Connected</span>
      <div class="kpi-value">${r.total_airports}</div>
      <span class="kpi-sub">${state.runways.length} runways &bull; ${state.gates.length} gates</span>
    </div>
    <div class="kpi-card accent-amber">
      <span class="kpi-label">Confirmed Bookings</span>
      <div class="kpi-value">${r.confirmed_bookings}</div>
      <span class="kpi-sub">${r.total_passengers} registered passengers</span>
    </div>
    <div class="kpi-card accent-emerald">
      <span class="kpi-label">Ticket Revenue</span>
      <div class="kpi-value">$${Number(r.total_revenue).toLocaleString()}</div>
      <span class="kpi-sub">${r.cancelled_bookings} cancelled bookings</span>
    </div>
  `;

  document.getElementById('reportsKpiGrid').innerHTML = `
    <div class="kpi-card"><span class="kpi-label">Total Airports</span><div class="kpi-value">${r.total_airports}</div></div>
    <div class="kpi-card"><span class="kpi-label">Total Aircraft</span><div class="kpi-value">${r.total_aircraft}</div></div>
    <div class="kpi-card accent-sky"><span class="kpi-label">Total Flights</span><div class="kpi-value">${r.total_flights}</div></div>
    <div class="kpi-card accent-rose"><span class="kpi-label">Cancelled Flights</span><div class="kpi-value">${r.cancelled_flights}</div></div>
    <div class="kpi-card"><span class="kpi-label">Total Passengers</span><div class="kpi-value">${r.total_passengers}</div></div>
    <div class="kpi-card accent-emerald"><span class="kpi-label">Confirmed Bookings</span><div class="kpi-value">${r.confirmed_bookings}</div></div>
    <div class="kpi-card accent-amber"><span class="kpi-label">Cancelled Bookings</span><div class="kpi-value">${r.cancelled_bookings}</div></div>
    <div class="kpi-card accent-emerald"><span class="kpi-label">Gross Revenue</span><div class="kpi-value">$${Number(r.total_revenue).toFixed(2)}</div></div>
  `;

  const totalF = Math.max(1, r.total_flights);
  document.getElementById('dashboardStatusBars').innerHTML = (r.flight_status_summary || []).map(item => {
    const pct = Math.round((item.count / totalF) * 100);
    return `
      <div style="margin-bottom:12px;">
        <div style="display:flex; justify-content:space-between; font-size:0.84rem; font-weight:600; margin-bottom:4px;">
          <span>${item.status}</span><span>${item.count} (${pct}%)</span>
        </div>
        <div style="height:9px; background:#E2E8F0; border-radius:99px; overflow:hidden;">
          <div style="width:${pct}%; height:100%; background:#0284C7;"></div>
        </div>
      </div>
    `;
  }).join('');

  const totalA = Math.max(1, r.total_aircraft);
  document.getElementById('dashboardFleetBars').innerHTML = (r.aircraft_status_summary || []).map(item => {
    const pct = Math.round((item.count / totalA) * 100);
    return `
      <div style="margin-bottom:12px;">
        <div style="display:flex; justify-content:space-between; font-size:0.84rem; font-weight:600; margin-bottom:4px;">
          <span>${item.status}</span><span>${item.count} (${pct}%)</span>
        </div>
        <div style="height:9px; background:#E2E8F0; border-radius:99px; overflow:hidden;">
          <div style="width:${pct}%; height:100%; background:#0B1F3A;"></div>
        </div>
      </div>
    `;
  }).join('');

  document.getElementById('reportFlightStatusBody').innerHTML = (r.flight_status_summary || []).map(s =>
    `<tr><td><span class="badge ${statusBadge(s.status)}">${s.status}</span></td><td><strong>${s.count}</strong></td></tr>`
  ).join('');

  document.getElementById('reportAircraftStatusBody').innerHTML = (r.aircraft_status_summary || []).map(s =>
    `<tr><td><span class="badge ${statusBadge(s.status)}">${s.status}</span></td><td><strong>${s.count}</strong></td></tr>`
  ).join('');

  document.getElementById('dashboardFlightsBody').innerHTML = state.flights.slice(0, 8).map(f => `
    <tr>
      <td><strong>${f.flight_number}</strong></td>
      <td>${f.airline_name}</td>
      <td><strong>${f.origin_iata}</strong> &rarr; <strong>${f.dest_iata}</strong></td>
      <td>${f.registration_no}</td>
      <td>Gate ${f.dep_gate_code} / RWY ${f.dep_runway_code}</td>
      <td>${f.scheduled_departure}</td>
      <td>${f.booked_seats} / ${f.total_seats}</td>
      <td><span class="badge ${statusBadge(f.status)}">${f.status}</span></td>
    </tr>
  `).join('');
}

function renderAirportsTable() {
  document.getElementById('airportsTableBody').innerHTML = state.airports.map(a => `
    <tr>
      <td><strong>${a.iata_code}</strong> / <code>${a.icao_code}</code></td>
      <td>${a.name}</td>
      <td>${a.city}, ${a.country}</td>
      <td>${a.timezone}</td>
      <td>${a.runway_count} Runways &bull; ${a.gate_count} Gates</td>
      <td><span class="badge ${statusBadge(a.status)}">${a.status}</span></td>
      <td>
        <button class="btn btn-outline btn-sm" onclick="editAirport(${a.airport_id})">Edit</button>
        <button class="btn btn-danger btn-sm" onclick="deleteAirport(${a.airport_id})">Delete</button>
      </td>
    </tr>
  `).join('');
}

function renderAircraftTable() {
  document.getElementById('aircraftTableBody').innerHTML = state.aircraft.map(ac => `
    <tr>
      <td><strong>${ac.registration_no}</strong></td>
      <td>${ac.model}</td>
      <td>${ac.manufacturer}</td>
      <td>${ac.total_seats} seats</td>
      <td>${ac.range_km.toLocaleString()} km</td>
      <td>${ac.current_airport_iata}</td>
      <td>${ac.active_flights}</td>
      <td><span class="badge ${statusBadge(ac.status)}">${ac.status}</span></td>
      <td>
        <button class="btn btn-outline btn-sm" onclick="editAircraft(${ac.aircraft_id})">Edit</button>
        <button class="btn btn-danger btn-sm" onclick="deleteAircraft(${ac.aircraft_id})">Delete</button>
      </td>
    </tr>
  `).join('');
}

function renderFlightsTable() {
  document.getElementById('flightsTableBody').innerHTML = state.flights.map(f => `
    <tr>
      <td><strong>${f.flight_number}</strong><br><small style="color:#64748B;">${f.airline_name}</small></td>
      <td><strong>${f.origin_iata}</strong> &rarr; <strong>${f.dest_iata}</strong></td>
      <td>${f.registration_no}<br><small style="color:#64748B;">${f.aircraft_model}</small></td>
      <td>${f.dep_gate_code} &rarr; ${f.arr_gate_code}</td>
      <td>${f.dep_runway_code} &rarr; ${f.arr_runway_code}</td>
      <td>${f.scheduled_departure}</td>
      <td>${f.scheduled_arrival}</td>
      <td><span class="badge badge-neutral">P${f.priority_level}</span></td>
      <td>$${Number(f.base_fare).toFixed(0)}</td>
      <td><span class="badge ${statusBadge(f.status)}">${f.status}</span></td>
      <td style="white-space:nowrap;">
        <button class="btn btn-outline btn-sm" onclick="openEditFlightModal(${f.flight_id})">Edit</button>
        ${f.status !== 'CANCELLED'
          ? `<button class="btn btn-danger btn-sm" onclick="cancelFlight(${f.flight_id})">Cancel</button>`
          : ''}
      </td>
    </tr>
  `).join('');
}

function renderPassengersTable() {
  document.getElementById('passengersTableBody').innerHTML = state.passengers.map(p => `
    <tr>
      <td><strong>${p.full_name}</strong></td>
      <td><code>${p.passport_number}</code></td>
      <td>${p.nationality}</td>
      <td>${p.email}</td>
      <td>${p.phone}</td>
      <td>${p.active_bookings}</td>
      <td>
        <button class="btn btn-outline btn-sm" onclick="editPassenger(${p.passenger_id})">Edit</button>
        <button class="btn btn-danger btn-sm" onclick="deletePassenger(${p.passenger_id})">Delete</button>
      </td>
    </tr>
  `).join('');
}

function renderResourcesTables() {
  document.getElementById('runwaysTableBody').innerHTML = state.runways.map(r => `
    <tr>
      <td><strong>${r.iata_code}</strong></td>
      <td>${r.runway_code}</td>
      <td>${r.length_meters} m</td>
      <td>${r.surface_type}</td>
      <td><span class="badge ${statusBadge(r.status)}">${r.status}</span></td>
    </tr>
  `).join('');

  document.getElementById('gatesTableBody').innerHTML = state.gates.map(g => `
    <tr>
      <td><strong>${g.iata_code}</strong></td>
      <td>${g.terminal}</td>
      <td>${g.gate_code}</td>
      <td>${g.gate_type}</td>
      <td><span class="badge ${statusBadge(g.status)}">${g.status}</span></td>
    </tr>
  `).join('');
}

async function fetchFilteredFlights() {
  const search = encodeURIComponent(document.getElementById('flightSearchInput').value.trim());
  const status = encodeURIComponent(document.getElementById('flightStatusFilter').value);
  const origin = encodeURIComponent(document.getElementById('flightOriginFilter').value);
  const res = await apiRequest(`/flights?search=${search}&status=${status}&origin=${origin}`);
  state.flights = res.data;
  renderFlightsTable();
}

window.refreshAllData = async function () {
  try {
    const [apRes, acRes, flRes, paxRes, resRes, repRes] = await Promise.all([
      apiRequest('/airports'),
      apiRequest('/aircraft'),
      apiRequest('/flights'),
      apiRequest('/passengers'),
      apiRequest('/resources'),
      apiRequest('/reports/summary')
    ]);

    state.airports = apRes.data;
    state.aircraft = acRes.data;
    state.flights = flRes.data;
    state.passengers = paxRes.data;
    state.runways = resRes.runways;
    state.gates = resRes.gates;
    state.report = repRes.report;

    populateAirportSelectors();
    populateAircraftSelectors();

    if (state.airports.length >= 2) {
      filterResourcesForAirport(state.airports[0].airport_id, 'chkDepGate', 'chkDepRunway');
      filterResourcesForAirport(state.airports[1].airport_id, 'chkArrGate', 'chkArrRunway');
      document.getElementById('chkDest').value = state.airports[1].airport_id;
    }

    renderDashboardAndReports();
    renderAirportsTable();
    renderAircraftTable();
    renderFlightsTable();
    renderPassengersTable();
    renderResourcesTables();
    await BookingsModule.loadBookingsAndSelectors(state.flights, state.passengers);
  } catch (err) {
    showToast(err.message, true);
  }
};

// CRUD Helpers exposed globally for inline table buttons
window.editAirport = (id) => {
  const a = state.airports.find(x => x.airport_id === id);
  if (!a) return;
  document.getElementById('editAirportId').value = a.airport_id;
  document.getElementById('apIata').value = a.iata_code;
  document.getElementById('apIcao').value = a.icao_code;
  document.getElementById('apName').value = a.name;
  document.getElementById('apCity').value = a.city;
  document.getElementById('apCountry').value = a.country;
  document.getElementById('apTimezone').value = a.timezone;
  document.getElementById('apStatus').value = a.status;
  document.getElementById('airportFormTitle').textContent = `Editing Airport: ${a.iata_code}`;
};

window.deleteAirport = async (id) => {
  if (!confirm('Delete this airport?')) return;
  try {
    const res = await apiRequest(`/airports/${id}`, { method: 'DELETE' });
    showToast(res.message);
    await window.refreshAllData();
  } catch (err) {
    showToast(err.message, true);
  }
};

window.editAircraft = (id) => {
  const ac = state.aircraft.find(x => x.aircraft_id === id);
  if (!ac) return;
  document.getElementById('editAircraftId').value = ac.aircraft_id;
  document.getElementById('acReg').value = ac.registration_no;
  document.getElementById('acModel').value = ac.model;
  document.getElementById('acMfg').value = ac.manufacturer;
  document.getElementById('acSeats').value = ac.total_seats;
  document.getElementById('acRange').value = ac.range_km;
  document.getElementById('acStatus').value = ac.status;
  document.getElementById('acAirport').value = ac.current_airport_id || 0;
  document.getElementById('aircraftFormTitle').textContent = `Editing Aircraft: ${ac.registration_no}`;
};

window.deleteAircraft = async (id) => {
  if (!confirm('Remove this aircraft from fleet?')) return;
  try {
    const res = await apiRequest(`/aircraft/${id}`, { method: 'DELETE' });
    showToast(res.message);
    await window.refreshAllData();
  } catch (err) {
    showToast(err.message, true);
  }
};

window.editPassenger = (id) => {
  const p = state.passengers.find(x => x.passenger_id === id);
  if (!p) return;
  document.getElementById('editPassengerId').value = p.passenger_id;
  document.getElementById('paxName').value = p.full_name;
  document.getElementById('paxEmail').value = p.email;
  document.getElementById('paxPhone').value = p.phone;
  document.getElementById('paxPassport').value = p.passport_number;
  document.getElementById('paxNationality').value = p.nationality;
  document.getElementById('paxDob').value = p.date_of_birth || '';
  document.getElementById('passengerFormTitle').textContent = `Editing Passenger: ${p.full_name}`;
};

window.deletePassenger = async (id) => {
  if (!confirm('Delete this passenger?')) return;
  try {
    const res = await apiRequest(`/passengers/${id}`, { method: 'DELETE' });
    showToast(res.message);
    await window.refreshAllData();
  } catch (err) {
    showToast(err.message, true);
  }
};

window.cancelFlight = async (id) => {
  if (!confirm('Cancel this flight and release its gate, runway, and aircraft allocations?')) return;
  try {
    const res = await apiRequest(`/flights/${id}/cancel`, { method: 'POST' });
    showToast(res.message);
    await window.refreshAllData();
  } catch (err) {
    showToast(err.message, true);
  }
};

function toLocalInputVal(mysqlDt) {
  return (mysqlDt || '').replace(' ', 'T').slice(0, 16);
}

window.openNewFlightModal = () => {
  document.getElementById('flightForm').reset();
  document.getElementById('editFlightId').value = '';
  document.getElementById('flightModalTitle').textContent = 'Schedule New Flight';
  document.getElementById('flightModalError').style.display = 'none';

  if (state.airports.length >= 2) {
    document.getElementById('flOrigin').value = state.airports[0].airport_id;
    document.getElementById('flDest').value = state.airports[1].airport_id;
    filterResourcesForAirport(state.airports[0].airport_id, 'flDepGate', 'flDepRunway');
    filterResourcesForAirport(state.airports[1].airport_id, 'flArrGate', 'flArrRunway');
  }
  document.getElementById('flDepTime').value = '2026-11-17T09:00';
  document.getElementById('flArrTime').value = '2026-11-17T11:30';
  document.getElementById('flightModal').classList.add('open');
};

window.openEditFlightModal = (id) => {
  const f = state.flights.find(x => x.flight_id === id);
  if (!f) return;
  document.getElementById('flightModalError').style.display = 'none';
  document.getElementById('flightModalTitle').textContent = `Edit Flight: ${f.flight_number}`;
  document.getElementById('editFlightId').value = f.flight_id;
  document.getElementById('flNumber').value = f.flight_number;
  document.getElementById('flAirline').value = f.airline_name;
  document.getElementById('flOrigin').value = f.origin_airport_id;
  document.getElementById('flDest').value = f.destination_airport_id;

  filterResourcesForAirport(f.origin_airport_id, 'flDepGate', 'flDepRunway');
  filterResourcesForAirport(f.destination_airport_id, 'flArrGate', 'flArrRunway');

  document.getElementById('flAircraft').value = f.aircraft_id;
  document.getElementById('flPriority').value = f.priority_level;
  document.getElementById('flDepGate').value = f.departure_gate_id;
  document.getElementById('flArrGate').value = f.arrival_gate_id;
  document.getElementById('flDepRunway').value = f.departure_runway_id;
  document.getElementById('flArrRunway').value = f.arrival_runway_id;
  document.getElementById('flDepTime').value = toLocalInputVal(f.scheduled_departure);
  document.getElementById('flArrTime').value = toLocalInputVal(f.scheduled_arrival);
  document.getElementById('flFare').value = f.base_fare;
  document.getElementById('flStatus').value = f.status;
  document.getElementById('flRemarks').value = f.remarks || '';
  document.getElementById('flightModal').classList.add('open');
};

document.addEventListener('DOMContentLoaded', async () => {
  if (!getAuthToken()) {
    window.location.href = 'login.html';
    return;
  }

  try {
    const me = await apiRequest('/auth/me');
    document.getElementById('topbarAdminName').textContent = me.admin.full_name;
    document.getElementById('sidebarAdminRole').textContent = `Role: ${me.admin.role}`;
  } catch {
    return;
  }

  // Navigation
  document.querySelectorAll('.nav-item').forEach(item => {
    item.addEventListener('click', () => switchView(item.dataset.view));
  });

  document.getElementById('menuToggle').addEventListener('click', () => {
    document.getElementById('sidebar').classList.toggle('open');
  });

  document.getElementById('logoutBtn').addEventListener('click', async () => {
    try { await apiRequest('/auth/logout', { method: 'POST' }); } catch {}
    localStorage.removeItem('flightops_token');
    localStorage.removeItem('flightops_admin');
    window.location.href = 'login.html';
  });

  // Flight Modal & Dynamic Gates/Runways
  document.getElementById('openNewFlightModalBtn').addEventListener('click', openNewFlightModal);
  document.getElementById('closeFlightModalBtn').addEventListener('click', () => document.getElementById('flightModal').classList.remove('open'));
  document.getElementById('cancelFlightModalBtn').addEventListener('click', () => document.getElementById('flightModal').classList.remove('open'));

  document.getElementById('flOrigin').addEventListener('change', (e) => {
    filterResourcesForAirport(e.target.value, 'flDepGate', 'flDepRunway');
  });
  document.getElementById('flDest').addEventListener('change', (e) => {
    filterResourcesForAirport(e.target.value, 'flArrGate', 'flArrRunway');
  });

  document.getElementById('chkOrigin').addEventListener('change', (e) => {
    filterResourcesForAirport(e.target.value, 'chkDepGate', 'chkDepRunway');
  });
  document.getElementById('chkDest').addEventListener('change', (e) => {
    filterResourcesForAirport(e.target.value, 'chkArrGate', 'chkArrRunway');
  });

  document.getElementById('applyFlightFiltersBtn').addEventListener('click', fetchFilteredFlights);

  // Save Flight Form
  document.getElementById('flightForm').addEventListener('submit', async (e) => {
    e.preventDefault();
    const errEl = document.getElementById('flightModalError');
    errEl.style.display = 'none';

    const editId = document.getElementById('editFlightId').value;
    const payload = {
      flight_number: document.getElementById('flNumber').value.trim(),
      airline_name: document.getElementById('flAirline').value.trim(),
      origin_airport_id: Number(document.getElementById('flOrigin').value),
      destination_airport_id: Number(document.getElementById('flDest').value),
      aircraft_id: Number(document.getElementById('flAircraft').value),
      priority_level: Number(document.getElementById('flPriority').value),
      departure_gate_id: Number(document.getElementById('flDepGate').value),
      arrival_gate_id: Number(document.getElementById('flArrGate').value),
      departure_runway_id: Number(document.getElementById('flDepRunway').value),
      arrival_runway_id: Number(document.getElementById('flArrRunway').value),
      scheduled_departure: document.getElementById('flDepTime').value,
      scheduled_arrival: document.getElementById('flArrTime').value,
      base_fare: Number(document.getElementById('flFare').value),
      status: document.getElementById('flStatus').value,
      remarks: document.getElementById('flRemarks').value.trim()
    };

    try {
      const res = editId
        ? await apiRequest(`/flights/${editId}`, { method: 'PUT', body: JSON.stringify(payload) })
        : await apiRequest('/flights', { method: 'POST', body: JSON.stringify(payload) });
      document.getElementById('flightModal').classList.remove('open');
      showToast(res.message);
      await window.refreshAllData();
    } catch (err) {
      errEl.textContent = err.message;
      errEl.style.display = 'block';
    }
  });

  // Conflict Dry-Run Form
  document.getElementById('conflictTestForm').addEventListener('submit', async (e) => {
    e.preventDefault();
    const alertBox = document.getElementById('conflictCheckAlert');
    try {
      const res = await apiRequest('/conflicts/check', {
        method: 'POST',
        body: JSON.stringify({
          origin_airport_id: Number(document.getElementById('chkOrigin').value),
          destination_airport_id: Number(document.getElementById('chkDest').value),
          aircraft_id: Number(document.getElementById('chkAircraft').value),
          departure_gate_id: Number(document.getElementById('chkDepGate').value),
          arrival_gate_id: Number(document.getElementById('chkArrGate').value),
          departure_runway_id: Number(document.getElementById('chkDepRunway').value),
          arrival_runway_id: Number(document.getElementById('chkArrRunway').value),
          scheduled_departure: document.getElementById('chkDepTime').value,
          scheduled_arrival: document.getElementById('chkArrTime').value
        })
      });
      alertBox.style.display = 'block';
      if (res.has_conflict) {
        alertBox.className = 'alert alert-danger';
        alertBox.textContent = `[CONFLICT DETECTED - ${res.conflict_type}] ${res.message}`;
      } else {
        alertBox.className = 'alert alert-success';
        alertBox.textContent = `[CLEAR] ${res.message}`;
      }
    } catch (err) {
      alertBox.style.display = 'block';
      alertBox.className = 'alert alert-danger';
      alertBox.textContent = err.message;
    }
  });

  // Add Runway / Gate
  document.getElementById('addRunwayForm').addEventListener('submit', async (e) => {
    e.preventDefault();
    try {
      const res = await apiRequest('/runways', {
        method: 'POST',
        body: JSON.stringify({
          airport_id: Number(document.getElementById('newRunwayAirport').value),
          runway_code: document.getElementById('newRunwayCode').value.trim(),
          length_meters: Number(document.getElementById('newRunwayLen').value)
        })
      });
      showToast(res.message);
      document.getElementById('newRunwayCode').value = '';
      await window.refreshAllData();
    } catch (err) {
      showToast(err.message, true);
    }
  });

  document.getElementById('addGateForm').addEventListener('submit', async (e) => {
    e.preventDefault();
    try {
      const res = await apiRequest('/gates', {
        method: 'POST',
        body: JSON.stringify({
          airport_id: Number(document.getElementById('newGateAirport').value),
          terminal: document.getElementById('newGateTerminal').value.trim(),
          gate_code: document.getElementById('newGateCode').value.trim(),
          gate_type: document.getElementById('newGateType').value
        })
      });
      showToast(res.message);
      document.getElementById('newGateCode').value = '';
      await window.refreshAllData();
    } catch (err) {
      showToast(err.message, true);
    }
  });

  // Airport Form
  document.getElementById('airportForm').addEventListener('submit', async (e) => {
    e.preventDefault();
    const id = document.getElementById('editAirportId').value;
    const payload = {
      iata_code: document.getElementById('apIata').value.trim(),
      icao_code: document.getElementById('apIcao').value.trim(),
      name: document.getElementById('apName').value.trim(),
      city: document.getElementById('apCity').value.trim(),
      country: document.getElementById('apCountry').value.trim(),
      timezone: document.getElementById('apTimezone').value.trim(),
      status: document.getElementById('apStatus').value
    };
    try {
      const res = id
        ? await apiRequest(`/airports/${id}`, { method: 'PUT', body: JSON.stringify(payload) })
        : await apiRequest('/airports', { method: 'POST', body: JSON.stringify(payload) });
      document.getElementById('airportForm').reset();
      document.getElementById('editAirportId').value = '';
      document.getElementById('airportFormTitle').textContent = 'Register / Update Airport';
      showToast(res.message);
      await window.refreshAllData();
    } catch (err) {
      showToast(err.message, true);
    }
  });

  document.getElementById('resetAirportBtn').addEventListener('click', () => {
    document.getElementById('airportForm').reset();
    document.getElementById('editAirportId').value = '';
    document.getElementById('airportFormTitle').textContent = 'Register / Update Airport';
  });

  // Aircraft Form
  document.getElementById('aircraftForm').addEventListener('submit', async (e) => {
    e.preventDefault();
    const id = document.getElementById('editAircraftId').value;
    const payload = {
      registration_no: document.getElementById('acReg').value.trim(),
      model: document.getElementById('acModel').value.trim(),
      manufacturer: document.getElementById('acMfg').value.trim(),
      total_seats: Number(document.getElementById('acSeats').value),
      range_km: Number(document.getElementById('acRange').value),
      status: document.getElementById('acStatus').value,
      current_airport_id: Number(document.getElementById('acAirport').value)
    };
    try {
      const res = id
        ? await apiRequest(`/aircraft/${id}`, { method: 'PUT', body: JSON.stringify(payload) })
        : await apiRequest('/aircraft', { method: 'POST', body: JSON.stringify(payload) });
      document.getElementById('aircraftForm').reset();
      document.getElementById('editAircraftId').value = '';
      document.getElementById('aircraftFormTitle').textContent = 'Add / Update Fleet Aircraft';
      showToast(res.message);
      await window.refreshAllData();
    } catch (err) {
      showToast(err.message, true);
    }
  });

  document.getElementById('resetAircraftBtn').addEventListener('click', () => {
    document.getElementById('aircraftForm').reset();
    document.getElementById('editAircraftId').value = '';
    document.getElementById('aircraftFormTitle').textContent = 'Add / Update Fleet Aircraft';
  });

  // Passenger Form
  document.getElementById('passengerForm').addEventListener('submit', async (e) => {
    e.preventDefault();
    const id = document.getElementById('editPassengerId').value;
    const payload = {
      full_name: document.getElementById('paxName').value.trim(),
      email: document.getElementById('paxEmail').value.trim(),
      phone: document.getElementById('paxPhone').value.trim(),
      passport_number: document.getElementById('paxPassport').value.trim(),
      nationality: document.getElementById('paxNationality').value.trim(),
      date_of_birth: document.getElementById('paxDob').value
    };
    try {
      const res = id
        ? await apiRequest(`/passengers/${id}`, { method: 'PUT', body: JSON.stringify(payload) })
        : await apiRequest('/passengers', { method: 'POST', body: JSON.stringify(payload) });
      document.getElementById('passengerForm').reset();
      document.getElementById('editPassengerId').value = '';
      document.getElementById('passengerFormTitle').textContent = 'Register / Edit Passenger';
      showToast(res.message);
      await window.refreshAllData();
    } catch (err) {
      showToast(err.message, true);
    }
  });

  document.getElementById('resetPassengerBtn').addEventListener('click', () => {
    document.getElementById('passengerForm').reset();
    document.getElementById('editPassengerId').value = '';
    document.getElementById('passengerFormTitle').textContent = 'Register / Edit Passenger';
  });

  BookingsModule.init();
  SchedulerModule.init();
  await window.refreshAllData();
});
