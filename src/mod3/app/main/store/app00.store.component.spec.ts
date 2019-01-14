import { async, ComponentFixture, TestBed } from '@angular/core/testing';

import { App00StoreComponent } from './app00.store.component';

describe('App00StoreComponent', () => {
  let component: App00StoreComponent;
  let fixture: ComponentFixture<App00StoreComponent>;

  beforeEach(async(() => {
    TestBed.configureTestingModule({
      declarations: [ App00StoreComponent ]
    })
    .compileComponents();
  }));

  beforeEach(() => {
    fixture = TestBed.createComponent(App00StoreComponent);
    component = fixture.componentInstance;
    fixture.detectChanges();
  });

  it('should create', () => {
    expect(component).toBeTruthy();
  });
});
