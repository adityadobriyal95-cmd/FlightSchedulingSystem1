document.addEventListener('DOMContentLoaded', () => {
  if (getAuthToken()) {
    window.location.href = 'index.html';
    return;
  }

  const fillDemoBtn = document.getElementById('fillDemoBtn');
  const loginForm = document.getElementById('loginForm');
  const loginError = document.getElementById('loginError');

  fillDemoBtn.addEventListener('click', () => {
    document.getElementById('username').value = 'admin';
    document.getElementById('password').value = 'Admin@123';
  });

  loginForm.addEventListener('submit', async (e) => {
    e.preventDefault();
    loginError.style.display = 'none';

    const username = document.getElementById('username').value.trim();
    const password = document.getElementById('password').value;

    try {
      const res = await apiRequest('/auth/login', {
        method: 'POST',
        body: JSON.stringify({ username, password })
      });
      localStorage.setItem('flightops_token', res.token);
      localStorage.setItem('flightops_admin', JSON.stringify(res.admin));
      window.location.href = 'index.html';
    } catch (err) {
      loginError.textContent = err.message;
      loginError.style.display = 'block';
    }
  });
});
