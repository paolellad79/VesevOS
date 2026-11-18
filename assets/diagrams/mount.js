
import dg from './diagram.js';

const go = () => dg.auto(document);
if (document.readyState === 'loading') document.addEventListener('DOMContentLoaded', go, { once: true });
else go();
