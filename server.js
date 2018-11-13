var express = require('express');
var app = express();
var server = require('http').createServer(app);
var io = require('socket.io')(server);
var path = require('path');
var config = {};
try {config=require('./config.json')} catch(err){};
var port = config.PORT || process.env.PORT || 3000;

server.listen(port, function () {
  console.log('Server listening at port %d', port);
});
app.use('/cmd/:command', function (req, res, next) {
  switch (req.params.command) {
    case 'act':
      console.log('/cmd/act');
      io.sockets.emit('server',{command: 'act'});
      res.send('server: {command: "act"}');
      break;
    default:
      res.send('Valid API-calls: <a href=/cmd/act>/cmd/act</a>');
  }
})
app.use(express.static(path.join(__dirname, 'public')));

io.on('connection', function (socket) {
  // socket.emit = reply only to the client who asked
  // socket.broadcast.emit = reply to all clients except the one who asked
  // io.sockets.emit = reply to all clients (including the one who asked)
  console.log(socket.id+' connected');
  socket.emit('_______server_______',{welcomemessage: 'Welcome!'});

  socket.on('broadcast', function (JSONobj) {
    socket.broadcast.emit(socket.id,JSON.stringify(JSONobj));
    console.log(socket.id+' '+JSON.stringify(JSONobj));

    if (JSONobj.get=='time') {
      let h=new Date().getHours(); if (h<10) {h='0'+h;}
      let m=new Date().getMinutes().toString(); if (m<10) {m='0'+m;}
      io.sockets.emit('_______server_______',{time: h+''+m});
      console.log('_______server_______ '+JSON.stringify({time: h+''+m}));
    }

  });

  socket.on('disconnect', (reason) => {
    console.log(socket.id+' disconnected: '+reason)
    if (reason === 'io server disconnect') {
      console.log('auto-reconnecting '+socket.id)
      socket.connect();
    } else {
      // else the socket will automatically try to reconnect
    }
  });

});

process.on('SIGINT', function(){console.log('SIGINT'); process.exit()});
process.on('SIGTERM', function(){console.log('SIGTERM'); process.exit()});
