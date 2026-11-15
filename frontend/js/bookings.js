window.BookingsModule = (() => {
  const ROWS = ['01', '02', '03', '04', '05', '06', '07', '08', '09', '10', '11', '12', '13', '14', '15'];
  const COLS = ['A', 'B', 'C', 'D', 'E', 'F'];

  async function renderSeatMap(flightId) {
    const grid = document.getElementById('cabinSeatGrid');
    if (!grid) return;
    if (!flightId) {
      grid.innerHTML = '<p style="grid-column:1/-1; color:#64748B; font-size:0.82rem;">Select a flight to inspect seat availability.</p>';
      return;
    }

    try {
      const res = await apiRequest(`/flights/${flightId}/seats`);
      const occupiedMap = new Map();
      (res.occupied_seats || []).forEach(s => occupiedMap.set(s.seat_number.toUpperCase(), s));

      const currentSelected = document.getElementById('bookSeatNumber').value.trim().toUpperCase();
      grid.innerHTML = '';

      ROWS.forEach(row => {
        COLS.forEach(col => {
          const seatCode = `${row}${col}`;
          const btn = document.createElement('button');
          btn.type = 'button';
          btn.textContent = seatCode;
          btn.className = 'seat-btn';

          if (occupiedMap.has(seatCode)) {
            const info = occupiedMap.get(seatCode);
            btn.classList.add('occupied');
            btn.disabled = true;
            btn.title = `Occupied by ${info.passenger_name} (${info.booking_reference})`;
          } else {
            if (seatCode === currentSelected) btn.classList.add('selected');
            btn.addEventListener('click', () => {
              document.getElementById('bookSeatNumber').value = seatCode;
              // Auto-suggest cabin class based on row
              const rNum = parseInt(row, 10);
              if (rNum <= 2) document.getElementById('bookSeatClass').value = 'FIRST';
              else if (rNum <= 6) document.getElementById('bookSeatClass').value = 'BUSINESS';
              else document.getElementById('bookSeatClass').value = 'ECONOMY';

              grid.querySelectorAll('.seat-btn').forEach(b => b.classList.remove('selected'));
              btn.classList.add('selected');
            });
          }
          grid.appendChild(btn);
        });
      });
    } catch (err) {
      grid.innerHTML = `<p style="grid-column:1/-1; color:#DC2626;">${err.message}</p>`;
    }
  }

  async function loadBookingsAndSelectors(flights, passengers) {
    const flightSel = document.getElementById('bookFlightSelect');
    const paxSel = document.getElementById('bookPassengerSelect');

    const activeFlights = flights.filter(f => f.status !== 'CANCELLED');
    flightSel.innerHTML = activeFlights.map(f =>
      `<option value="${f.flight_id}">${f.flight_number} (${f.origin_iata} &rarr; ${f.dest_iata}) | $${f.base_fare} | ${f.scheduled_departure}</option>`
    ).join('');

    paxSel.innerHTML = passengers.map(p =>
      `<option value="${p.passenger_id}">${p.full_name} (${p.passport_number})</option>`
    ).join('');

    if (activeFlights.length > 0) {
      await renderSeatMap(flightSel.value);
    }

    const res = await apiRequest('/bookings');
    const tbody = document.getElementById('bookingsTableBody');
    tbody.innerHTML = res.data.map(b => {
      const badge = b.booking_status === 'CONFIRMED' ? 'badge-success' : 'badge-danger';
      const cancelAction = b.booking_status === 'CONFIRMED'
        ? `<button class="btn btn-danger btn-sm" onclick="BookingsModule.cancelBooking(${b.booking_id})">Cancel</button>`
        : '<span style="color:#94A3B8;">Released</span>';
      return `
        <tr>
          <td><strong>${b.booking_reference}</strong></td>
          <td>${b.flight_number}</td>
          <td>${b.route}</td>
          <td>${b.passenger_name}</td>
          <td><code>${b.passport_number}</code></td>
          <td><strong>${b.seat_number}</strong></td>
          <td>${b.seat_class}</td>
          <td>$${Number(b.fare_paid).toFixed(2)}</td>
          <td><span class="badge ${badge}">${b.booking_status}</span></td>
          <td>${cancelAction}</td>
        </tr>
      `;
    }).join('');
  }

  async function cancelBooking(bookingId) {
    if (!confirm('Cancel this booking and release the seat?')) return;
    try {
      const res = await apiRequest(`/bookings/${bookingId}/cancel`, { method: 'POST' });
      showToast(res.message);
      await window.refreshAllData();
    } catch (err) {
      showToast(err.message, true);
    }
  }

  function init() {
    const flightSel = document.getElementById('bookFlightSelect');
    flightSel.addEventListener('change', () => renderSeatMap(flightSel.value));

    document.getElementById('bookingForm').addEventListener('submit', async (e) => {
      e.preventDefault();
      const payload = {
        flight_id: Number(document.getElementById('bookFlightSelect').value),
        passenger_id: Number(document.getElementById('bookPassengerSelect').value),
        seat_class: document.getElementById('bookSeatClass').value,
        seat_number: document.getElementById('bookSeatNumber').value.trim().toUpperCase()
      };
      try {
        const res = await apiRequest('/bookings', {
          method: 'POST',
          body: JSON.stringify(payload)
        });
        showToast(res.message);
        document.getElementById('bookSeatNumber').value = '';
        await window.refreshAllData();
      } catch (err) {
        showToast(err.message, true);
      }
    });
  }

  return { init, loadBookingsAndSelectors, cancelBooking, renderSeatMap };
})();
