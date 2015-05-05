"use strict"

let net = require( "net" )

function main() {
	console.log( "connecting" )
	let n1, n2
	n1 = net.connect( 7832, "37.200.65.233" )
	n1.once( "main_reconnect", function () { setTimeout( main, 300 ) } )
	n1.once( "main_done", function () {
		n1 && ( n1.end(), n1 = null )
		n2 && ( n2.end(), n2 = null )
		buffer = null
	} )
	n1.on( "end", n1.emit.bind( n1, "main_reconnect" ) )
	n1.on( "error", n1.emit.bind( n1, "main_reconnect" ) )
	n1.on( "data", n1.emit.bind( n1, "main_reconnect" ) )
	n1.on( "end", n1.emit.bind( n1, "main_done" ) )
	n1.on( "error", n1.emit.bind( n1, "main_done" ) )
	let buffer = []
	n1.on( "data", function ( data ) {
		if ( !n1 )
			return
		buffer.push( data )
		if ( !n2 ) {
			n2 = net.connect( 22 )
			n2.on( "end", n1.emit.bind( n1, "main_done" ) )
			n2.on( "error", n1.emit.bind( n1, "main_done" ) )
			n2.on( "connect", function () {
				if ( !n1 )
					return
				n1.removeAllListeners( "data" )
				n1.pipe( n2 )
				n2.pipe( n1 )
				n1.write( Buffer.concat( buffer ) )
				buffer = null
			} )
		}
	} )
}

main()
