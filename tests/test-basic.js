
const common = require('./common');
const d3bUtil = require('../client-nodejs/d3b-util');

module.exports = {

testConnect: common.defaultTest((test, client) => {
	test.expect(0);
}),

testUpload: common.defaultTest((test, client) => {
	test.expect(1);

	var upload = Buffer.from('Hello world!');

	return d3bUtil.uploadExtern(client, {
		fileName: 'upload.txt',
		buffer: upload
	})
	.then(() => {
		return d3bUtil.downloadExtern(client, {
			fileName: 'upload.txt'
		})
		.then(download => {
			test.ok(upload.equals(download),
					"Upload/download mismatch");
		});
	});
})

};

