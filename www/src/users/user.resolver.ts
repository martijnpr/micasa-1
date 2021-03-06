import { Injectable }      from '@angular/core';
import {
	Router,
	Resolve,
	RouterStateSnapshot,
	ActivatedRouteSnapshot
}                          from '@angular/router';
import { Observable }      from 'rxjs/Observable';

import {
	User,
	UsersService,
	ACL
}                          from './users.service';

@Injectable()
export class UserResolver implements Resolve<User> {

	public constructor(
		private _router: Router,
		private _usersService: UsersService
	) {
	};

	public resolve( route_: ActivatedRouteSnapshot, state_: RouterStateSnapshot ): Observable<User> {
		var me = this;
		if ( route_.params['user_id'] == 'add' ) {
			return Observable.of( { id: NaN, name: 'New user', username: '', rights: ACL.Viewer, enabled: false } );
		} else {
			return this._usersService.getUser( +route_.params['user_id'] )
				.catch( function( error_: string ) {
					me._router.navigate( [ '/login' ] );
					return Observable.of( null );
				} )
			;
		}
	};
}
