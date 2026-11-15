const API_BASE = window.location.origin.startsWith('http')
  ? `${window.location.origin}/api`
  : 'http://localhost:8080/api';

function getAuthToken() {
  return localStorage.getItem('flightops_token') || '';
}

function showToast(message, isError = false) {
  const el = document.getElementById('toast');
  if (!el) return;
  el.textContent = message;
  el.className = isError ? 'toast error show' : 'toast show';
  clearTimeout(window.__toastTimer);
  window.__toastTimer = setTimeout(() => {
    el.className = 'toast';
  }, 4000);
}

async function apiRequest(endpoint, options = {}) {
  const headers = {
    'Content-Type': 'application/json',
    ...(options.headers || {})
  };
  const token = getAuthToken();
  if (token) {
    headers['Authorization'] = `Bearer ${token}`;
  }

  const response = await fetch(`${API_BASE}${endpoint}`, {
    ...options,
    headers
  });

  if (response.status === 401 && !endpoint.startsWith('/auth/login')) {
    localStorage.removeItem('flightops_token');
    localStorage.removeItem('flightops_admin');
    window.location.href = 'login.html';
    throw new Error('Session expired. Redirecting to login.');
  }

  const data = await response.json();
  if (!response.ok || data.success === false) {
    throw new Error(data.error || `Request failed (${response.status})`);
  }
  return data;
}
