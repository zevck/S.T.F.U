import React from 'react';
import ReactDOM from 'react-dom/client';
import { App } from './app';
import './index.css';
import { SKSE_API, log } from './lib/skse-api';

// Initialize SKSE API bridge
log('[index.tsx] Starting STFU UI initialization...');
SKSE_API.init();
log('[index.tsx] SKSE_API.init() completed');

ReactDOM.createRoot(document.getElementById('root')!).render(
  <React.StrictMode>
    <App />
  </React.StrictMode>
);

log('[index.tsx] React app rendered');
