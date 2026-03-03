import './App.css';
import { BrowserRouter as Router, Routes, Route, Link } from 'react-router-dom';
import Settings from './components/Settings';
import HomePage from './components/HomePage';

function App() {
  return (
    <Router>
      <div className="App">
        <nav className="bg-gray-800 text-white p-4">
          <div className="container mx-auto flex space-x-4">
            <Link to="/" className="px-3 py-2 rounded hover:bg-gray-700">ホーム</Link>
            <Link to="/settings" className="px-3 py-2 rounded hover:bg-gray-700">設定</Link>
          </div>
        </nav>
        
        <Routes>
          <Route path="/" element={<HomePage />} />
          <Route path="/settings" element={<Settings />} />
        </Routes>
      </div>
    </Router>
  );
}

export default App;
