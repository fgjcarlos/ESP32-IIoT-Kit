import { h, render } from 'preact';
import { App } from './app.js';
import './styles/app.css';

const contenedor = document.getElementById('app');
if (contenedor) {
  render(h(App), contenedor);
}
